/* Copyright (c) 2013 Xingxing Ke <yykxx@hotmail.com>
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

#include <netdb.h>
#include <errno.h>

#include "plm_log.h"
#include "plm_http_plugin.h"
#include "plm_http_forward.h"

static void plm_http_forward_send(void *, int);
static void plm_http_forward_connect(void *, int);
static void plm_http_forward_close(void *);

void plm_http_forward_start(struct plm_http_request *request, void *data,
							void (*fn)(void *, int))
{
	const char *errmsg;
	struct plm_http_conn *conn;
	struct plm_mempool *pool;
	struct plm_http_forward *forward;
	struct hostent *ent;
	
	ent = gethostbyname(request->hr_host.s_str);
	if (!ent) {
		errmsg = "gethostbyname failed";
		plm_log_write(PLM_LOG_FATAL, "%s %s %d: %s",
					  __FILE__, __FUNCTION__, __LINE__, errmsg);
		fn(data, -1);
		return;
	}
		
	conn = (struct plm_http_conn *)request->hr_conn;
	pool = &conn->hc_pool;
	forward = plm_mempool_alloc(pool, sizeof(*forward));   
	if (forward) {
		memset(forward, 0, sizeof(*forward));
		forward->hf_addr.sin_family = AF_INET;
		forward->hf_addr.sin_port = htons(request->hr_port);
		forward->hf_addr.sin_addr = *(struct in_addr *)ent->h_addr;
		forward->hf_fn = fn;
		forward->hf_data = data;
		forward->hf_request = request;
		forward->hf_send_header = 1;
		forward->hf_fd =
			plm_comm_open(PLM_COMM_TCP, NULL, 0, 0, 0, NULL, 0, 1, 0);
		
		if (forward->hf_fd > 0) {
			forward->hf_cch.cch_handler = plm_http_forward_close;
			forward->hf_cch.cch_data = forward;
			plm_comm_add_close_handler(forward->hf_fd, &forward->hf_cch);
			request->hr_forward = forward;
			plm_http_forward_connect(forward, forward->hf_fd);
			return;
		}

		errmsg = "plm_comm_open failed";
	} else {
		errmsg = "plm_mempool_alloc failed";
	}

	plm_log_write(PLM_LOG_FATAL, "%s %s %d: %s %s",
				  __FILE__, __FUNCTION__, __LINE__, errmsg, strerror(errno));
	fn(data, -1);
}

void plm_http_forward_body(struct plm_http_request *request, void *data,
						   void (*fn)(void *, int))
{
	struct plm_http_forward *forward;

	forward = request->hr_forward;
	forward->hf_fn = fn;
	forward->hf_data = data;
	forward->hf_send_body = 1;
	forward->hf_offset = 0;

	plm_http_forward_send(forward, forward->hf_fd);
}

static void plm_http_forward_send(void *data, int fd)
{
	int n, offset;
	plm_string_t *buf;
	const char *errmsg = NULL;
	struct plm_http_request *request;
	struct plm_http_forward *forward;

	forward = (struct plm_http_forward *)data;
	request = forward->hf_request;

	if (!forward->hf_post_recv) {
		if (!plm_event_io_read(fd, forward, plm_http_response_read)) {
			forward->hf_post_recv = 1;
		} else {
			forward->hf_fn(forward->hf_data, -1);
			return;
		}
	}	

	if (forward->hf_send_body)
		buf = &request->hr_bodybuf;
	else if (forward->hf_send_header)
		buf = &request->hr_reqbuf;
	
	offset = request->hr_offset;
	n = plm_comm_write(fd, buf->s_str + offset, buf->s_len - offset);
	if (n < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) {
			if (plm_event_io_write(fd, forward, plm_http_forward_send))
				errmsg = "not ready plm_event_io_write failed";
		} else {
			errmsg = "plm_comm_write failed";
		}
	} else {
		request->hr_offset += n;

		/* send all data complete */
		if (request->hr_offset == buf->s_len) {
			forward->hf_fn(forward->hf_data, 0);
		} else {
			if (plm_event_io_write(fd, forward, plm_http_forward_send))
				errmsg = "not complete plm_event_io_write failed";
		}
	}

	if (errmsg) {
		forward->hf_fn(forward->hf_data, -1);
		plm_log_write(PLM_LOG_FATAL, "%s %s %d: %s %s", __FILE__, __FUNCTION__,
					  __LINE__, errmsg, strerror(errno));
	}
}

void plm_http_forward_close(void *data)
{
	struct plm_http_forward *forward;

	forward = (struct plm_http_forward *)data;
}

void plm_http_forward_connect(void *data, int fd)
{
	int err;
	const char *errmsg = NULL;
	struct plm_http_forward *forward;

	forward = (struct plm_http_forward *)data;
	err = plm_comm_connect(fd, &forward->hf_addr);
	if (err == 0 || errno == EISCONN) {
		plm_http_forward_send(forward, fd);
	} else {
		int err = 0;
		
		if (errno == EINPROGRESS || errno == EWOULDBLOCK || errno == EAGAIN
			|| errno == EALREADY || errno == EINTR || errno == ERESTART)
			err = plm_event_io_write(fd, data, plm_http_forward_connect);
		else
			err = -1;
		
		if (err) {
			forward->hf_fn(forward->hf_data, -1);
			plm_comm_close(forward->hf_fd);
			errmsg = "plm_comm_connect failed";
		}
	}

	if (errmsg) {
		plm_log_write(PLM_LOG_FATAL, "%s %s %d: %s %s", __FILE__, __FUNCTION__,
					  __LINE__, errmsg, strerror(errno));
	}
}

