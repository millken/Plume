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

#include "plm_string.h"
#include "plm_list.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PLM_HTTP_PARSE_DONE 0
#define PLM_HTTP_PARSE_AGAIN 1	

typedef enum {
	PLM_METHOD_GET,
	PLM_METHOD_HEAD,
	PLM_METHOD_POST,
	PLM_METHOD_PUT,
	PLM_METHOD_CONNECT,
	PLM_METHOD_TRACE,
	PLM_METHOD_END
} plm_method_t;

typedef enum {
	PLM_PROTO_HTTP,
	PLM_PROTO_END
} plm_proto_t;

struct plm_http_comm {
	/* only request */
	plm_method_t hc_method;
	/* only response */
	int hc_status;
	plm_proto_t hc_proto;
	struct {
		unsigned char hc_maj_version;
		unsigned char hc_min_version;
	};

	/* internal state */
	int hc_parser_state;
};

typedef void (*parser_delegate)(const char *, int, struct plm_http_comm *);	
	
void plm_http_proto_delegate_set(parser_delegate on_url,
								 parser_delegate on_key,
								 parser_delegate on_value);

void plm_http_proto_init(struct plm_http_comm *http);
	
int plm_http_proto_parse(int *bytes_parsed, struct plm_http_comm *http,
						 const char *buf, int n);	

#ifdef __cplusplus
}
#endif

#endif
