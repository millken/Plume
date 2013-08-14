/* Copyright (c) 2011-2013 Xingxing Ke <yykxx@hotmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "plm_comm.h"
#include "plm_event.h"
#include "plm_log.h"
#include "plm_buffer.h"
#include "plm_http.h"
#include "plm_http_plugin.h"
#include "plm_http_response.h"
#include "plm_http_request.h"

static void plm_http_read_header(void *, int);
static void plm_http_read_body(void *, int);
static void plm_http_request_chain_process(struct plm_http_request *);
static struct plm_http_conn *
plm_http_conn_alloc(struct plm_http_ctx *, struct sockaddr_in *, int);
static void plm_http_conn_free(void *);

static struct plm_http_request *
plm_http_request_alloc(struct plm_http_conn *conn);

void plm_http_accept(void *data, int fd)
{
	int retry = 0;
	int clifd, err;
	struct sockaddr_in addr;
	struct plm_http_ctx *ctx;
	struct plm_http_conn *conn;

	do {
		ctx = (struct plm_http_ctx *)data;

		clifd = plm_comm_accept(fd, &addr, 1);
		if (clifd < 0) {
			int level = retry > 0 ? PLM_LOG_DEBUG : PLM_LOG_FATAL;
			plm_log_write(level, "plm_comm_accept failed: %s", strerror(errno));
			break;
		}

		retry++;
		conn = plm_http_conn_alloc(ctx, &addr, clifd);
		if (!conn) {
			plm_comm_close(clifd);
			plm_log_write(PLM_LOG_FATAL, "plm_http_conn_alloc failed");
			break;
		}

		plm_comm_add_close_handler(clifd, &conn->hc_close_handler);
		err = plm_event_io_read(clifd, conn, plm_http_read_header);
		if (err) {
			plm_comm_close(clifd);
			plm_log_write(PLM_LOG_FATAL, "plm_event_io_read on new fd "
						  "failed: %s", strerror(errno));
			break;
		}
	} while (1);

	err = plm_event_io_read(fd, data, plm_http_accept);
	if (err) {
		plm_log_write(PLM_LOG_FATAL, "plm_event_io_read on listen fd "
					  "failed: %s", strerror(errno));
	}   
}

void plm_http_reply(void *data, int fd)
{
	int n;
	char *buf;
	size_t bufsize;
	struct plm_http_conn *conn;
	struct plm_http_request *request;
	struct plm_http_forward *forward;

	conn = (struct plm_http_conn *)data;
	request = conn->hr_request;
	forward = (struct plm_http_forward *)request->hr_forward;

	if (forward->hf_offset == 0) {
		plm_comm_close(fd);
		return;
	}

	buf = forward->hf_buf;
	bufsize = forward->hf_offset - request->hr_reply_offset;
	n = plm_comm_write(fd, buf, bufsize);
	if (n < 0) {
		int err = 0;
		
		if (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR)
			err = plm_event_io_write(fd, data, plm_http_reply);
		else
			err = -1;

		if (err) {
			plm_log_write(PLM_LOG_FATAL, "plm_comm_write failed: %s",
						  strerror(errno));
			plm_comm_close(fd);
		}
		return;
	}

	request->hr_reply_offset += n;
	if (request->hr_reply_offset < forward->hf_offset) {
		plm_event_io_write(fd, data, plm_http_reply);
		return;
	}

	request->hr_reply_offset = forward->hf_offset = 0;
	plm_http_response_read(forward, forward->hf_fd);
}

struct plm_http_conn *plm_http_conn_alloc(struct plm_http_ctx *ctx,
										  struct sockaddr_in *addr,
										  int fd)
{
	struct plm_http_conn *conn;

	conn = (struct plm_http_conn *)
		plm_lookaside_list_alloc(&ctx->hc_conn_pool, NULL);
	if (conn) {
		conn->hc_buf = plm_buffer_alloc(MEM_4K);
		if (conn->hc_buf) {
			conn->hc_size = SIZE_4K;
			conn->hc_offset = 0;
			
			conn->hc_fd = fd;
			memcpy(&conn->hc_addr, addr, sizeof(conn->hc_addr));

			plm_mempool_init(&conn->hc_pool, 512, malloc, free);
			plm_http_init(&conn->hc_request.hr_http, PLM_HTTP_REQUEST,
						  &conn->hc_pool);
		
			conn->hc_close_handler.cch_handler = plm_http_conn_free;
			conn->hc_close_handler.cch_data = conn;
		} else {
			conn->hc_size = conn->hc_offset = 0;
		}		
	}
	
	return (conn);
}

void plm_http_conn_free(void *data)
{
	struct plm_http_conn *conn;

	conn = (struct plm_http_conn *)data;
	if (conn->hc_buf) {
		plm_buffer_free(MEM_4K, conn->hc_buf);
		plm_mempool_destroy(&conn->hc_pool);
	}
}

static int
plm_http_request_parse(struct plm_http_request **pp, size_t *parsed,
					   size_t n, struct plm_http_conn *conn)
{
	int rc;
	struct plm_http_request *req;

	req = *pp;
	if (!req || req->hr_complete) {
		req = plm_http_request_alloc(conn);
		*pp = req;
	}

	req->hr_next = conn->hc_request;
	conn->hc_request = req;
	return plm_http_parse(parsed, &req->hr_http, conn->hc_buf, n);
}

void plm_http_read_header(void *data, int fd)
{
	int header_done = 0;
	struct plm_http_conn *conn;
	struct plm_http_request *request;

	conn = (struct plm_http_conn *)data;
	for (;;) {
		int rc, n;
		size_t parsed;

		n = plm_comm_read(fd, conn->hc_buf + conn->hc_offset,
						  conn->hc_size - conn->hc_offset);
		if (n == EWOULDBLOCK)
			break;
		
		if (n == 0) {
			/* EOF */
		}

		rc = plm_http_request_parse(&request, &parsed, n, conn);
		if (rc == PLM_HTTP_PARSE_DONE) {
			header_done = 1;
			break;
		} else if (rc == PLM_HTTP_PARSE_ERROR) {
			/* indicate bad request */
			break;
		}
		
		conn->hc_offset = n - parsed;
		if (parsed != n)
			memmove(conn->hc_buf, conn->hc_buf + parsed, conn->hc_offset);
	}

	if (header_done) {
		if (request->hr_hasbody) {
			if (plm_event_io_read(fd, data, plm_http_read_body)) {
				plm_comm_close(fd);
				plm_log_write(PLM_LOG_FATAL, "plm_event_io_read failed: %s",
							  strerror(errno));
				return;
			}
		}
		plm_http_request_chain_process(&request);		
	} else {
		if (plm_event_io_read(fd, data, plm_http_read_header)) {
			plm_comm_close(fd);
			plm_log_write(PLM_LOG_FATAL, "plm_event_io_read failed: %s",
						  strerror(errno));
		}		
	}
}

static void plm_http_body_complete(void *, int);
static void plm_http_read_body(void *data, int fd)
{
	int n;
	struct plm_http_conn *conn;
	struct plm_http_request *request;

	conn = (struct plm_http_conn *)data;
	n = plm_comm_read(fd, conn->hc_buf, conn->hc_size);
	if (n == 0) {
		/* client closed */
		return;
	}
	
	request = conn->hc_request;
	request->hr_bodybuf.s_str = conn->hc_buf;
	request->hr_bodybuf.s_len = conn->hc_size;

	if (request->hr_send_header_done)
		plm_http_forward_body(request, request, plm_http_body_complete);
}

static void plm_http_buf_pack(void *key, void *value, void *data)
{
	struct plm_http_request *request;
	struct plm_http_conn *conn;	
	struct plm_mempool *pool;
	plm_string_t *k, *v, *buf;

	request = (struct plm_http_request *)data;
	conn = (struct plm_http_conn *)request->hr_conn;
	pool = &conn->hc_pool;
	buf = &request->hr_reqbuf;
	k = (plm_string_t *)key;
	v = (plm_string_t *)value;

	plm_strappend_field(buf, k, v, pool);
}

static int plm_http_request_pack(struct plm_http_request *request)
{
	struct plm_http_conn *conn;	
	struct plm_mempool *pool;
	plm_string_t *buf;
	plm_string_t *url;

	request = (struct plm_http_request *)data;
	conn = (struct plm_http_conn *)request->hr_conn;
	pool = &conn->hc_pool;
	buf = &request->hr_reqbuf;
	url = &request->hr_http.h_url;

	/* request line */
	plm_strappend2(buf, plm_http_method(&request->hr_http), pool);
	plm_strappend(buf, " ", 1, pool);
	plm_strappend(buf, url->s_str, url->s_len, pool);
	plm_strappend(buf, " ", 1, pool);
	plm_strappend2(buf, http_verion, pool);
	plm_strappend(buf, "\r\n", 2, pool);

	/* field */
	plm_hash_foreach(&request->hr_http.h_fields, request, plm_http_buf_pack);

	/* end \r\n */
	plm_strappend(buf, "\r\n", 2, pool);
	return (0);
}

void plm_http_body_complete(void *data, int state)
{
	int err;
	struct plm_http_request *request;
	struct plm_http_conn *con;
	
	if (state == -1) {
		/* error */
		return;
	}

	request = (struct plm_http_request *)data;
	conn = (struct plm_http_conn *)request->hr_conn;

	err = plm_event_io_read(conn->hc_fd, conn, plm_http_read_body);
	if (err) {
		/* error */
	}
}

static void plm_http_request_complete(void *data, int state)
{
	plm_string_t *buf;
	struct plm_http_request *request;
	
	if (state == -1) {
		/* error */
		return;
	}

	request->hr_send_header_done = 1;
	request = (struct plm_http_request *)data;
	buf = &request->hr_bodybuf;
	if (buf->s_len > 0) {
		request->hr_offset = 0;
		plm_http_forward_body(request, data, plm_http_body_complete);
	}
}

void plm_http_request_chain_process(struct plm_http_request *request)
{
	if (!plm_http_request_pack(request)) {
		plm_http_forward_start(request, request, plm_http_request_complete);
	} else {
		plm_log_write(PLM_LOG_FATAL, "plm_http_request_pack failed");
		plm_http_request_send_complete(request, -1);
	}
}

struct plm_http_request *plm_http_request_alloc(struct plm_http_conn *conn)
{
	struct plm_mempool *pool;
	struct plm_http_request *request;

	pool = conn->hc_pool;
	request = plm_mempool_alloc(pool, sizeof(*request));
	if (request)
		memset(request, 0, sizeof(*request));
	
	return (request);
}

