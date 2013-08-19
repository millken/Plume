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

#ifndef _PLM_HTTP_FORWARD_H
#define _PLM_HTTP_FORWARD_H

#include <stdint.h>
#include <arpa/inet.h>

#include "plm_comm.h"
#include "plm_http.h"
#include "plm_http_response.h"

#ifdef __cplusplus
extern "C" {
#endif

struct plm_http_forward {
	uint16_t hf_send_header : 1;
	uint16_t hf_send_body : 1;
	uint16_t hf_post_recv : 1;

	int hf_fd;

	/* buffer for recv orign server reply */
	char *hf_buf;
	size_t hf_size;
	size_t hf_offset;

	/* orign server */
	struct sockaddr_in hf_addr;

	/* fd close handler */
	struct plm_comm_close_handler hf_cch;

	struct plm_http_request *hf_request;

	void (*hf_fn)(void *, int);
	void *hf_data;
};

void plm_http_request_forward(struct plm_http_request *request,
							  void (*fn)(void *data, int));

void plm_http_body_forward(struct plm_http_request *request,
							  void (*fn)(void *data, int));

void plm_http_connect_forward(struct plm_http_request *request,
							  void (*fn)(void *data, int));
	
#ifdef __cplusplus
}
#endif

#endif
