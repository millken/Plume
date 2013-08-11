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
#include "http_parser.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PLM_HTTP_PARSE_DONE 0
#define PLM_HTTP_PARSE_AGAIN 1
#define PLM_HTTP_PARSE_ERROR (-1)

enum plm_http_type {
	PLM_HTTP_NONE,
	PLM_HTTP_REQUEST,
	PLM_HTTP_RESPONSE
};

enum plm_last_state {
	PLM_LAST_NONE,
	PLM_LAST_KEY,
	PLM_LAST_VALUE
};

struct plm_http_field {
	struct plm_hash_node hf_node;
	#define hf_key hf_node.hn_key
	#define hf_value hf_node.hn_value
};	

struct plm_http {
	struct http_parser h_parser;
	
	uint8_t h_header_done : 1;
	uint8_t h_status_line_done : 1;

	plm_string_t h_url;
	struct plm_hash h_fields;
	struct plm_mempool *h_pool;
	plm_string_t *h_last_key;
	plm_string_t *h_last_value;
	int8_t h_last_state;

	#define h_maj_version h_parser.http_major
	#define h_min_version h_parser.http_minor
	#define h_status_code h_parser.status_code
	#define h_method h_parser.method
};

/* init http structure, return 0 on success, else -1 */	
int plm_http_init(struct plm_http *http, enum plm_http_type type,
				   struct plm_mempool *pool);

/* parse http
 * @parsed -- bytes parsed
 * @http -- http
 * @buf -- http data
 * @len -- length of buf in bytes
 * returns
 *   PLM_HTTP_PARSE_DONE indicate complete
 *   PLM_HTTP_PARSE_AGAIN indicate need more data
 *   PLM_HTTP_PARSE_ERROR indicate parse error
 */	
int plm_http_parse(size_t *parsed, struct plm_http *http,
				   const char *buf, size_t len);

/* return a static buffer string on success, else NULL */	
const char *plm_http_parse_error(struct plm_http *http);

#define plm_http_parse_status_line_done(http) (http)->h_status_line_done
#define plm_http_parse_header_done(http) (http)->h_header_done

int plm_http_parser_request_line_done(struct plm_http *http);

#ifdef __cplusplus
}
#endif

#endif
