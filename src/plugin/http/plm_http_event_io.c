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

#include "plm_http_event_io.h"

static void
plm_http_event_write_cb(void *data, int fd)
{
	int n;
	struct plm_http_wrevt *we;

	we = (struct plm_http_wrevt *)data;
	n = plm_comm_write(fd, we->hw_buf + we->hw_off, we->hw_len - we->hw_off);
	if (n > 0) {
		we->hw_off += n;
		if (we->hw_len > we->hw_off)
			PLM_EVT_DRV_WRITE(fd, we, plm_http_event_write_cb);
		else
			we->hw_fn(we->hw_data, we->hw_buf, we->hw_len, 0);
	} else if (n <= 0) {
		if (plm_comm_ignore(errno))
			PLM_EVT_DRV_WRITE(fd, we, plm_http_event_write_cb);
		else
			we->hw_fn(we->hw_data, we->hw_buf, we->hw_off, -1);
	}
}

void plm_http_event_write(int fd, struct plm_http_wrevt *we)
{
	int n;

	n = plm_comm_write(fd, we->hw_buf, we->hw_len);
	if (n > 0) {
		we->hw_off += n;
		if (we->hw_len > we->hw_off)
			PLM_EVT_DRV_WRITE(fd, we, plm_http_event_write_cb);
		else
			we->hw_fn(we->hw_data, we->hw_buf, we->hw_len, 0);
	} else if (n <= 0) {
		if (plm_comm_ignore(errno))
			PLM_EVT_DRV_WRITE(fd, we, plm_http_event_write_cb);
		else
			we->hw_fn(we->hw_data, we->hw_buf, we->hw_off, -1);
	}
}
