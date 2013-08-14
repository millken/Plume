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

#include "plm_http_forward.h"

static void plm_http_forward_send(struct plm_http_forward *forward);
static void plm_http_forward_connect(void *, int);

void plm_http_forward_start(struct plm_http_request *request, void *data,
							void (*fn)(void *, int))
{
	struct hostent *ent;
	struct plm_http_conn *conn;
	struct plm_mempool *pool;
	struct plm_http_forward *forward;

	conn = (struct plm_http_conn *)request->hr_conn;
	pool = &conn->hc_pool;
	forward = plm_mempool_alloc(pool, sizeof(*forward));
	if (forward) {
		ent = gethostbyname(request->hr_host.s_str);
		if (!ent) {
			plm_log_write(PLM_LOG_FATAL, "gethostbyname failed");
			goto ERR;
		}
		
		memset(forward, 0, sizeof(*forward));
		request->hr_forward = forward;
		forward->hf_fn = fn;
		forward->hf_data = data;
		forward->hf_remote = *(struct in_addr *)ent->h_addr;
		forward->hf_port = request->hr_port;
		forward->hf_request = request;
		forward->hf_send_header = 1;
		forward->hf_fd = plm_comm_open(PLM_COMM_TCP, NULL, 0, 0,
									   0, NULL, 0, 1, 0);
		if (forward->hf_fd < 0) {
			plm_log_write(PLM_LOG_FATAL, "plm_comm_open failed: %s",
						  strerror(errno));
			goto ERR;
		}
			
		if (plm_event_io_write(forward->hf_fd,
							   forward, plm_http_forward_connect))
			goto ERR;
		
		return;
	}

ERR:
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

	plm_http_forward_send(forward);
}

static void plm_http_forward_send_data(void *data, int fd)
{
	int n, offset;
	plm_string_t *buf;
	struct plm_http_request *request;
	struct plm_http_forward *forward;

	forward = (struct plm_http_forward *)data;
	request = (struct plm_http_request *)forward->hf_request;

	if (!forward->hf_post_recv) {
		/* second chance */
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
		if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR)
			return plm_http_forward_send(forward);
		forward->hf_fn(forward->hf_data, -1);
	} else {
		request->hr_offset += n;

		/* send all data complete */
		if (request->hr_offset == buf->s_len)
			forward->hf_fn(forward->hf_data, 0);
	}
}

void plm_http_forward_send(struct plm_http_forward *forward)
{
	int fd, err;

	fd = forward->hf_fd;
	err = plm_event_io_write(fd, forward, plm_http_forward_send_data);
	if (err) {
		plm_log_write(PLM_LOG_FATAL, "plm_http_forward_send failed: %s",
					  strerror(errno));
		forward->hf_fn(forward->hf_data, -1);
	}

	if (!forward->hf_post_recv) {
		if (!plm_event_io_read(fd, forward, plm_http_response_read))
			forward->hf_post_recv = 1;
		else
			plm_log_write(PLM_LOG_FATAL, "plm_event_io_read failed: %s",
						  strerror(errno));
	}
}

void plm_http_forward_connect(void *data, int fd)
{
	int err;
	struct sockaddr_in addr;
	struct plm_http_forward *forward;

	forward = (struct plm_http_forward *)data;
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(forward->hf_port);
	addr.sin_addr.s_addr = forward->hf_addr;

	err = plm_comm_connect(forward->hf_fd, &addr);
	if (err == 0 || errno == EISCONN) {
		plm_http_forward_send(forward);
	} else {
		int err = 0;
		
		if (errno == EINPROGRESS || errno == EWOULDBLOCK || errno == EAGAIN
			|| errno == EALREADY || errno == EINTR || errno == ERESTART)
			err = plm_event_io_write(fd, data, plm_http_forward_connect);
		else
			err = -1;
		
		if (err) {
			plm_log_write(PLM_LOG_FATAL, "plm_http_forward_connect failed: %s",
						  strerror(errno));
			forward->hf_fn(forward->hf_data, -1);
		}
	}
}

