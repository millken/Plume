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

#include "plm_http.h"

static int
plm_http_on_msg_begin(struct http_parser *);

static int
plm_http_on_url(struct http_parser *, const char *, size_t);

static int
plm_http_on_status_complete(struct plm_http_parser *);

static int
plm_http_on_header_field(struct plm_http_parser *, const char *, size_t);

static int
plm_http_on_header_value(struct plm_http_parser *, const char *, size_t);

static int
plm_http_on_headers_complete(struct plm_http_parser *);

static int
plm_http_on_body(struct plm_http_parser *, const char *, size_t);

static int
plm_http_on_msg_complete(struct http_parser *);

static struct http_parser_settings parser_settings = {
	plm_http_on_msg_begin,
	plm_http_on_url,
	plm_http_on_status_complete,
	plm_http_on_header_field,
	plm_http_on_header_value,
	plm_http_on_headers_complete,
	plm_http_on_body,
	plm_http_on_msg_complete
};

/* init http structure with specifiy type */	
void plm_http_init(struct plm_http *http, enum plm_http_type type)
{
	enum http_parser_type ptype;

	if (type == PLM_HTTP_REQUEST)
		ptype = HTTP_REQUEST;
	else if (type == PLM_HTTP_RESPONSE)
		ptype = HTTP_RESPONSE;
	else
		ptype = HTTP_BOTH;

	http_parser_init(&http->h_parser, ptype);
}

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
				   const char *buf, size_t len)
{
	size_t n = http_parser_execute(&http->h_parser, &parser_settings, buf, len);
	if (n != len) {
		/* the header must be pared done */
		if (!http->h_header_done)
			return (PLM_HTTP_PARSE_ERROR);
	}
}
