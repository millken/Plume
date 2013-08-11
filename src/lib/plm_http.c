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
plm_http_on_status_complete(struct http_parser *);

static int
plm_http_on_header_field(struct http_parser *, const char *, size_t);

static int
plm_http_on_header_value(struct http_parser *, const char *, size_t);

static int
plm_http_on_headers_complete(struct http_parser *);

static int
plm_http_on_body(struct http_parser *, const char *, size_t);

static int
plm_http_on_msg_complete(struct http_parser *);

static uint32_t plm_http_field_key(void *, uint32_t);
static int plm_http_field_cmp(void *, void *);
static void *plm_http_alloc_bucket(size_t, void *);
static void plm_http_free_bucket(void *, void *);

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

/* init http structure, return 0 on success, else -1 */	
int plm_http_init(struct plm_http *http, enum plm_http_type type,
				   struct plm_mempool *pool)
{
	enum http_parser_type ptype;

	if (type == PLM_HTTP_REQUEST)
		ptype = HTTP_REQUEST;
	else if (type == PLM_HTTP_RESPONSE)
		ptype = HTTP_RESPONSE;
	else
		ptype = HTTP_BOTH;

	http_parser_init(&http->h_parser, ptype);
	http->h_pool = pool;
	http->h_header_done = http->h_status_line_done = 0;
	memset(&http->h_url, 0, sizeof(http->h_url));
	http->h_last_key = http->h_last_value = NULL;
	http->h_last_state = PLM_LAST_NONE;
	
	return plm_hash_init(&http->h_fields, 26,
						 plm_http_field_key, plm_http_field_cmp,
						 plm_http_alloc_bucket, plm_http_free_bucket, http);
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
	int rc;
	size_t n;

	n = http_parser_execute(&http->h_parser, &parser_settings, buf, len);

	if (!http->h_header_done) {
		if (n == len)
			rc = PLM_HTTP_PARSE_AGAIN;
		else
			rc = PLM_HTTP_PARSE_ERROR;
	} else {
		rc = PLM_HTTP_PARSE_DONE;
	}

	*parsed = n;
	return (rc);
}

const char *plm_http_parse_error(struct plm_http *http)
{
	return http_errno_description(HTTP_PARSER_ERRNO(&http->h_parser));
}

int plm_http_parser_request_line_done(struct plm_http *http)
{
	int done = 0;
	
	if (http->h_parser.type == HTTP_REQUEST) {
		unsigned short maj, min;

		maj = http->h_parser.http_major;
		min = http->h_parser.http_minor;

		switch (maj) {
		case 0:
			if (min == 9)
				done = 1;
			break;

		case 1:
			if (min == 0 || min == 1)
				done = 1;
			break;
		}
	}

	return (done);
}

int plm_http_on_msg_begin(struct http_parser *parser)
{
	return (0);
}

int plm_http_on_url(struct http_parser *parser,
					const char *buf, size_t len)
{
	struct plm_http *http;

	http = (struct plm_http *)parser;
	plm_strassign(&http->h_url, buf, len, http->h_pool);
	return (0);
}

int plm_http_on_status_complete(struct http_parser *parser)
{
	struct plm_http *http;

	http = (struct plm_http *)parser;
	http->h_status_line_done = 1;
	return (0);
}

int plm_http_on_header_field(struct http_parser *parser,
							 const char *buf, size_t len)
{
	struct plm_http *http;

	http = (struct plm_http *)parser;
	if (http->h_last_state != PLM_LAST_KEY)
		plm_stralloc(&http->h_last_key, buf, len, http->h_pool);
	else
		plm_strappend(http->h_last_key, buf, len, http->h_pool);

	http->h_last_state = PLM_LAST_KEY;
	return (0);
}

int plm_http_on_header_value(struct http_parser *parser,
							 const char *buf, size_t len)
{
	int rc = 0;
	struct plm_http *http;
	struct plm_http_field *field;

	http = (struct plm_http *)parser;
	if (http->h_last_state == PLM_LAST_KEY) {
		field = (struct plm_http_field *)
			plm_mempool_alloc(http->h_pool, sizeof(struct plm_http_field));
		if (field) {
			plm_stralloc(&http->h_last_value, buf, len, http->h_pool);
			field->hf_key = http->h_last_key;
			field->hf_value = http->h_last_value;
			plm_hash_insert(&http->h_fields, &field->hf_node);
		} else {
			rc = -1;
		}
	} else {
		/* the field key had been insert into hash tabel */
		plm_strappend(http->h_last_key, buf, len, http->h_pool);
	}

	http->h_last_state = PLM_LAST_VALUE;
	return (rc);
}

int plm_http_on_headers_complete(struct http_parser *parser)
{
	struct plm_http *http;

	http = (struct plm_http *)parser;
	http->h_header_done = 1;
	return (1);
}

int plm_http_on_body(struct http_parser *parser,
					 const char *buf, size_t len)
{
	return (0);
}

int plm_http_on_msg_complete(struct http_parser *parser)
{
	return (0);
}

uint32_t plm_http_field_key(void *data, uint32_t max)
{
	plm_string_t *key;

	key = (plm_string_t *)data;
	return (key->s_str[0] % max);
}

int plm_http_field_cmp(void *v1, void *v2)
{
	plm_string_t *value1, *value2;

	value1 = (plm_string_t *)v1;
	value2 = (plm_string_t *)v2;

	return plm_strcmp(value1, value2);
}

void *plm_http_alloc_bucket(size_t sz, void *data)
{
	return plm_mempool_alloc(((struct plm_http *)data)->h_pool, sz);
}

void plm_http_free_bucket(void *ptr, void *data)
{
}
