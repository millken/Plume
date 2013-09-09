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

#ifndef _PLM_HTTP_PARSER_H
#define _PLM_HTTP_PARSER_H

#include <stdint.h>

#include "plm_string.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PLM_HTTP_PARSE_DONE (0)
#define PLM_HTTP_PARSE_AGAIN (1)
#define PLM_HTTP_PARSE_ERROR (-1)
#define PLM_HTTP_PARSE_BREAK (-2)

#define PLM_HTTP_VERSION09 "HTTP/0.9"
#define PLM_HTTP_VERSION10 "HTTP/1.0"
#define PLM_HTTP_VERSION11 "HTTP/1.1"

enum plm_http_ver {
	PLM_HTTP_VNONE,
	PLM_HTTP_09,
	PLM_HTTP_10,
	PLM_HTTP_11
};

enum plm_http_mthd {
	PLM_MTHD_NONE,
	PLM_MTHD_CONNECT,
	PLM_MTHD_DELETE,
	PLM_MTHD_GET,
	PLM_MTHD_HEAD,
	PLM_MTHD_POST,
	PLM_MTHD_PUT,
	PLM_MTHD_OPTIONS,
	PLM_MTHD_TRACE
};

typedef struct plm_http_parser {
	/* parse state */
	int hp_state;
	
	/* bytes parsed every time */
	size_t hp_parsed;

	/* user data */
	void *hp_data;

	/* return 0 indicate success, else -1 to break parse */
	
	int (*hp_on_req_line)(enum plm_http_mthd, const plm_string_t *,
						  enum plm_http_ver, void *);
	
	int (*hp_on_status_line)(enum plm_http_ver, int, plm_string_t *, void *);

	int (*hp_on_field)(const plm_string_t *, const plm_string_t *, void *);
	
	void (*hp_on_hdr_done)(void *);
	
} plm_http_parser_t;

struct plm_http_url {
	plm_string_t hu_scheme;
	plm_string_t hu_host;
	plm_string_t hu_port;
	plm_string_t hu_path;
};

#define plm_http_parser_init(p, v) ((p)->hp_state = 0, (p)->hp_data = (v))

int plm_http_parser_req(plm_http_parser_t *parser, plm_string_t *s);

int plm_http_parser_resp(plm_http_parser_t *parser, plm_string_t *s);

int plm_http_parser_url(struct plm_http_url *out, const plm_string_t *url);

#ifdef __cplusplus
}
#endif

#endif
