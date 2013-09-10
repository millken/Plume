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

#ifndef _PLM_HTTP_PROTO_H
#define _PLM_HTTP_PROTO_H

#include <stdint.h>

#include "plm_hash.h"
#include "plm_string.h"
#include "plm_mempool.h"
#include "plm_list.h"
#include "plm_comm.h"
#include "plm_http_event_io.h"
#include "plm_http_parser.h"
#include "plm_http_plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

struct plm_http_body {
	void *hb_data;
	void (*hb_callback)(void *, plm_string_t *);
};		

struct plm_http_conn {
	plm_list_t hc_reqs;
	plm_list_t hc_resps;
	
	struct plm_mempool hc_pool;
	struct plm_comm_close_handler hc_cch;
	plm_http_parser_t hc_parser;
	
	int hc_fd;
	struct sockaddr_in hc_addr;

	struct plm_http_body hc_body;
	
	struct {
		char *hc_data;
		size_t hc_size;
		size_t hc_offset;
	} hc_in;

	struct plm_http_wrevt hc_wrevt;

	plm_list_t hc_free_reqs;
	plm_list_t hc_free_resps;

	struct {
		uint8_t hc_eof : 1;
		uint8_t hc_badreq : 1;
	} hc_flags;

	struct plm_http_ctx *hc_ctx;
};

struct plm_http_req {
	plm_list_node_t hr_node;

	enum plm_http_mthd hr_mthd;
	enum plm_http_ver hr_ver;
	plm_string_t hr_url;
	struct plm_hash hr_fields;

	uint64_t hr_cntlen;
	plm_string_t hr_host;
	uint16_t hr_port;

	struct plm_http_conn *hr_conn;
	struct sockaddr_in *hr_backend;

	struct {
		uint8_t hr_keepalive : 1;
		uint8_t hr_pipeline : 1;
		uint8_t hr_hdr_kpalv_on : 1;
	} hr_flags;
};

struct plm_http_resp {
	plm_list_node_t hr_node;

	enum plm_http_ver hr_ver;
	int hr_status;
	plm_string_t hr_desc;
	struct plm_hash hr_fields;

	struct plm_http_conn *hr_conn;
	struct plm_http_req *hr_req;

	struct {
		uint8_t hr_keepalive : 1;
		uint8_t hr_pipeline : 1;
	} hr_flags;
};

#ifdef __cplusplus
}
#endif

#endif
