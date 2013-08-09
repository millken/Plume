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

struct plm_http {
	struct http_parser h_parser;
	
	/* set when parsed http header done */
	unsigned char h_header_done : 1;
	
	/* set when content_length set in header */
	unsigned char h_has_body : 1;

	/* set when received all data of body */
	unsigned char h_body_done : 1;

	/* set when parsed all message */
	unsigned char h_msg_complete : 1;
};

/* init http structure with specifiy type */	
void plm_http_init(struct plm_http *http, enum plm_http_type type);	

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
const char *plm_http_parse_error(sturct plm_http *http);	

#ifdef __cplusplus
}
#endif

#endif
