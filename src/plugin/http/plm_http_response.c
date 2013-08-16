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

#include <errno.h>

#include "plm_log.h"
#include "plm_buffer.h"
#include "plm_comm.h"
#include "plm_http.h"
#include "plm_http_plugin.h"
#include "plm_http_request.h"
#include "plm_http_forward.h"
#include "plm_http_response.h"

void plm_http_response_read(void *data, int fd)
{
	int n;
	char *buf;
	size_t bufsize;
	const char *errmsg = NULL;
	struct plm_http_forward *forward;
	struct plm_http_request *request;

	forward = (struct plm_http_forward *)data;
	if (!forward->hf_buf) {
		forward->hf_buf = plm_buffer_alloc(MEM_8K);
		if (!forward->hf_buf) {
			errmsg = "plm_buffer_alloc failed";
			goto ERR;
		}
		forward->hf_size = SIZE_8K;
		forward->hf_offset = 0;
	}

	buf = forward->hf_buf + forward->hf_offset;
	bufsize = forward->hf_size - forward->hf_offset;

	n = plm_comm_read(fd, buf, bufsize);
	if (n < 0) {
		if (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR) {
			if (plm_event_io_read(fd, data, plm_http_response_read))
				errmsg = "plm_event_io_read failed";
		} else {
			errmsg = "plm_comm_read failed";
		}

		if (errmsg)
			goto ERR;
		
		return;
	}

	if (n == 0)
		plm_comm_close(fd);

	forward->hf_offset = n;
	request = forward->hf_request;
	plm_http_reply(request->hr_conn, request->hr_conn->hc_fd);
	return;

ERR:
	plm_log_write(PLM_LOG_FATAL, "%s %s %d: %s %s",
				  __FILE__, __FUNCTION__, __LINE__, errmsg, strerror(errno));
	plm_comm_close(forward->hf_fd);
	forward->hf_offset = 0;
	plm_http_reply(request->hr_conn, request->hr_conn->hc_fd);
}
