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
	struct plm_http_forward *forward;
	struct plm_http_request *request;
	struct plm_http_conn *conn;

	forward = (struct plm_http_forward *)data;
	if (!forward->hr_buf) {
		forward->hr_buf = plm_buffer_alloc(MEM_8K);
		if (!forward->hf_buf) {
			/* indicate client error */
			plm_log_write(PLM_LOG_FATAL, "plm_buffer_alloc failed");
			return;
		}
	}

	buf = forward->hr_buf + forward->hr_offset;
	bufsize = forward->hr_size - forward->hr_offset;

	n = plm_comm_read(fd, buf, bufsize);
	if (n < 0) {
		int err = 0;
		
		if (errno == EWOULDBLOCK || errno == EAGAIN || errno == EINTR)
			err = plm_event_io_read(fd, data, plm_http_response_read);
		else
			err = -1;

		if (err) {
			plm_log_write(PLM_LOG_FATAL, "plm_comm_read failed: %s",
						  strerror(errno));
			plm_comm_close(fd);
		}
		return;
	}

	if (n == 0) {
		/* EOF */
	}

	forward->hf_offset = n;

	request = (struct plm_http_request *)forward->hf_request;
	conn = (struct plm_http_conn *)request->hr_conn;

	plm_http_reply(conn, conn->hc_fd);
}
