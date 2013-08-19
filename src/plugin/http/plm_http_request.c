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
#include "plm_http_forward.h"
#include "plm_http_response.h"
#include "plm_http_request.h"

static int http_server;

static void plm_http_read_header(void *, int);
static void plm_http_read_body(void *, int);
static void plm_http_body_forward_done(void *, int);
static void plm_http_chain_process(struct plm_http_request *);
static struct plm_http_conn *plm_http_conn_alloc(struct plm_http_ctx *);
static void plm_http_conn_free(void *);
static struct plm_http_request *plm_http_request_alloc(struct plm_http_conn *);
static void plm_http_reset(struct plm_http_conn *);
static void plm_http_bad_request(struct plm_http_conn *);
static void plm_http_bad_gateway(struct plm_http_conn *);

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
			plm_log_write(level, "plm_comm_accept failed: %s",
						  strerror(errno));
			break;
		}

		retry++;
		conn = plm_http_conn_alloc(ctx);
		if (!conn) {
			plm_comm_close(clifd);
			plm_log_write(PLM_LOG_FATAL, "plm_http_conn_alloc failed");
			break;
		}

		conn->hc_fd = clifd;
		conn->hc_addr = addr;
		plm_comm_add_close_handler(clifd, &conn->hc_cch);
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
}

struct plm_http_conn *plm_http_conn_alloc(struct plm_http_ctx *ctx)
{
	struct plm_http_conn *conn;

	conn = (struct plm_http_conn *)
		plm_lookaside_list_alloc(&ctx->hc_conn_pool, NULL);
	if (conn) {
		memset(conn, 0, sizeof(*conn));
		conn->hc_buf = plm_buffer_alloc(MEM_4K);
		if (conn->hc_buf) {
			conn->hc_size = SIZE_8K;
			plm_mempool_init(&conn->hc_pool, 512, malloc, free);
			conn->hc_cch.cch_handler = plm_http_conn_free;
			conn->hc_cch.cch_data = conn;
		} else {
			plm_lookaside_list_free(&ctx->hc_conn_pool, conn, NULL);
			conn = NULL;
		}		
	}
	
	return (conn);
}

void plm_http_conn_free(void *data)
{
	struct plm_http_conn *conn;

	conn = (struct plm_http_conn *)data;
	if (conn->hc_buf) {
		plm_buffer_free(MEM_8K, conn->hc_buf);
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
	if (!req || req->hr_http.h_header_done) {
		req = plm_http_request_alloc(conn);
		*pp = req;

		if (plm_http_init(&req->hr_http, HTTP_REQUEST, &conn->hc_pool))
			return (-1);

		PLM_LIST_ADD_FRONT(&conn->hc_list, &req->hr_node);
	}

	return plm_http_parse(parsed, &req->hr_http, conn->hc_buf, n);
}

void plm_http_read_header(void *data, int fd)
{
	int rc, n;
	size_t parsed;	
	struct plm_http_conn *conn;
	struct plm_http_request *request;

	conn = (struct plm_http_conn *)data;
	request = conn->hc_request;
	n = plm_comm_read(fd, conn->hc_buf + conn->hc_offset,
					  conn->hc_size - conn->hc_offset);
	if (n < 0) {
		if (plm_comm_ignore(errno)) {
			plm_event_io_read(fd, data, plm_http_read_header);
		} else {
			plm_log_write(PLM_LOG_FATAL, "%s: read error, %s",
						  __FUNCTION__, strerror(errno));
			plm_comm_close(fd);
		}
		return;
	}

	if (n == 0) {
		/* EOF */
		plm_http_reset(conn, request);
		return;
	}

	rc = plm_http_request_parse(&request, &parsed, n, conn);
	if (rc == PLM_HTTP_PARSE_ERROR) {
		plm_log_write(PLM_LOG_FATAL, "%s: bad request", __FUNCTION__);
		plm_http_bad_request(conn, request);
		return;
	}
		
	conn->hc_offset = n - parsed;
	if (parsed != n)
		memmove(conn->hc_buf, conn->hc_buf + parsed, conn->hc_offset);

	if (rc == PLM_HTTP_PARSE_DONE) {
		if (request->hr_hasbody)
			plm_event_io_read(fd, data, plm_http_read_body);
		plm_http_chain_process(request);		
	} else {
		plm_event_io_read(fd, data, plm_http_read_header);
	}
}

static void plm_http_read_body(void *data, int fd)
{
	int n;
	struct plm_http_conn *conn;
	struct plm_http_request *request;

	conn = (struct plm_http_conn *)data;
	n = plm_comm_read(fd, conn->hc_buf, conn->hc_size);
	if (n == 0) {
		/* client closed */
		plm_http_reset(conn);
		return;
	}

	request = (struct plm_http_request *)PLM_LIST_FRONT(&conn->hc_list);
	request->hr_bodybuf.s_str = conn->hc_buf;
	request->hr_bodybuf.s_len = n;

	if (request->hr_send_header_done) {
		plm_http_body_forward(request, plm_http_body_forward_done);
		plm_event_io_read(fd, data, plm_http_read_body);
	}
}

void plm_http_body_forward_done(void *data, int state)
{
	struct plm_http_request *request;
	struct plm_http_conn *conn;

	request = (struct plm_http_request *)data;
	conn = request->hr_conn;
	
	if (state == -1) {
		plm_log_write(PLM_LOG_FATAL, "%s: state indicate failed", __FUNCTION__);
		plm_http_bad_gateway(request->hc_conn);
	} else {
		plm_log_write(PLM_LOG_DEBUG, "%s: body forwad done", __FUNCTION__);
	}
}

static void plm_http_request_forward_done(void *data, int state)
{
	plm_string_t *buf;
	struct plm_http_request *request;

	request = (struct plm_http_request *)data;	
	if (state == -1) {
		plm_log_write(PLM_LOG_FATAL, "%s state indicate failed", __FUNCTION__);
		plm_http_bad_gateway(request->hr_conn);
		return;
	}

	request->hr_send_header_done = 1;
	buf = &request->hr_bodybuf;
	if (buf->s_len > 0) {
		request->hr_offset = 0;
		plm_http_body_forward(request, plm_http_body_forward_done);
		plm_event_io_read(request->hr_conn->hc_fd, request->hc_conn,
						  plm_http_read_body);
	}
}

static void plm_http_request_parse_field(struct plm_http_request *request)
{
	int state = 0;
	plm_string_t *v;	
	struct plm_http *http;
	struct plm_hash_node *node;
	struct plm_http_field *host, *connection;
	plm_string_t key_host = plm_string("Host");
	plm_string_t key_conn = plm_string("Connection");
	plm_string_t key_proxy_conn = plm_string("Proxy-Connection");

	http = &request->hr_http;

	if (!plm_hash_find(&node, &http->h_fields, &key_host)) {
		host = (struct plm_http_field *)node;
		request->hr_host = *(plm_string_t *)host->hf_value;
	}

	if (!plm_hash_find(&node, &http->h_fields, &key_conn))
		state = 1;
	
	if (!plm_hash_find(&node, &http->h_fields, &key_proxy_conn))
		state = 2;
	
	if (state != 0) {
		connection = (struct plm_http_field *)node;
		v = (plm_string_t *)connection->hf_value;
		if (strcasestr(v->s_str, "Keep-Alive"))
			request->hr_keepalive = 1;
		if (state == 2)
			plm_hash_delete(&http->h_fields, &key_proxy_conn);
	}
	request->hr_port = 80;
}

static void plm_http_buf_pack(void *key, void *value, void *data)
{
	struct plm_http_request *request;
	struct plm_mempool *pool;
	plm_string_t *k, *v, *buf;

	request = (struct plm_http_request *)data;
	pool = &request->hr_conn->hc_pool;
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
	char http_version[20];

	conn = (struct plm_http_conn *)request->hr_conn;
	pool = &conn->hc_pool;
	buf = &request->hr_reqbuf;
	url = &request->hr_http.h_url;

	snprintf(http_version, sizeof(http_version), "HTTP/%d.%d",
			 request->hr_http.h_maj_version, request->hr_http.h_min_version);

	/* request line */
	plm_strappend2(buf, plm_http_method(&request->hr_http), pool);
	plm_strappend(buf, " ", 1, pool);
	plm_strappend(buf, url->s_str, url->s_len, pool);
	plm_strappend(buf, " ", 1, pool);
	plm_strappend2(buf, http_version, pool);
	plm_strappend(buf, "\r\n", 2, pool);

	/* field */
	plm_hash_foreach(&request->hr_http.h_fields, request, plm_http_buf_pack);
	
	/* end \r\n */
	plm_strzappend(buf, "\r\n", 2, pool);
	return (0);
}

void plm_http_chain_process(struct plm_http_request *request)
{
	plm_http_request_parse_field(request);
	if (!plm_http_request_pack(request)) {
		switch (request->hr_http.h_method) {
		case HTTP_GET:
		case HTTP_HEAD:
		case HTTP_POST:
		case HTTP_PUT:
			plm_http_request_forward(request, plm_http_request_forward_done);
			break;
		case HTTP_CONNECT:
			plm_http_connect_forward(request, plm_http_connect_forward_done);
			break;
		}
	} else {
		plm_log_write(PLM_LOG_FATAL, "plm_http_request_pack failed");
		plm_http_request_forward_done(request, -1);
	}
}

struct plm_http_request *plm_http_request_alloc(struct plm_http_conn *conn)
{
	struct plm_mempool *pool;
	struct plm_http_request *request;

	pool = &conn->hc_pool;
	request = plm_mempool_alloc(pool, sizeof(*request));
	if (request) {
		memset(request, 0, sizeof(*request));
		request->hr_conn = conn;
	}
	
	return (request);
}

int plm_http_open_server(struct plm_http_ctx *ctx);
{
	int err = -1;
	int port = ctx->hc_port;
	int backlog = ctx->hc_backlog;
	const char *ip = ctx->hc_addr.s_str;
	
	http_server = plm_comm_open(PLM_COMM_TCP, NULL, 0, 0, port, ip,
								backlog, 1, 1);
	if (http_server < 0) {
		plm_log_syslog("can't open http plugin listen fd: %s:%d", ip, port);
	} else {
		err = plm_event_io_read(http_server, ctx, plm_http_accept);
		if (err)
			plm_log_syslog("plm_event_io_read failed on listen fd");
	}

	return (err);
}

int plm_http_close_server()
{
	int err = 0;
	
	if (http_server > 0) {
		err = plm_comm_close(http_server);
		http_server = -1;
	}

	return (err);
}

