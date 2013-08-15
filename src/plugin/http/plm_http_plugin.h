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

#ifndef _PLM_HTTP_PLUGIN_H
#define _PLM_HTTP_PLUGIN_H

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <stdint.h>

#include "plm_comm.h"
#include "plm_mempool.h"
#include "plm_lookaside_list.h"
#include "plm_dlist.h"
#include "plm_http.h"

#ifdef __cplusplus
extern "C" {
#endif

struct plm_http_ctx {
	int hc_fd;
	plm_string_t hc_addr;
	short hc_port;
	int hc_backlog;
	struct plm_lookaside_list hc_conn_pool;
};

struct plm_http_conn {
	struct plm_http_request *hc_request;
	struct plm_mempool hc_pool;
	struct sockaddr_in hc_addr;
	struct plm_comm_close_handler hc_cch;
	
	int hc_fd;
	char *hc_buf;
	size_t hc_size;
	size_t hc_offset;
};

#ifdef __cplusplus
}
#endif	

#endif
