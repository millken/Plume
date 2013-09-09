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

#include <assert.h>
#include "plm_http_parser.h"

enum plm_parser_state {
	PLM_PRS_NONE,
	PLM_PRS_RL_CR,
	PLM_PRS_RL_LF,
	PLM_PRS_RL_DONE,
	PLM_PRS_SL_CR,
	PLM_PRS_SL_LF,
	PLM_PRS_SL_DONE,
	PLM_PRS_HDR_CR,
	PLM_PRS_HDR_LF,
	PLM_PRS_HDR_DONE
};

static int
plm_http_parser_req_line(plm_http_parser_t *psr, plm_string_t *s)
{
	enum plm_http_mthd mthd = PLM_MTHD_NONE;
	char *str = s->s_str, *lf, *p;
	size_t len = s->s_len, i;
	plm_string_t url, verstr;
	enum plm_http_ver ver = PLM_HTTP_VNONE;

	for (i = 0; i < len && str[i] == ' '; i++) /* none */;

	if (i > 0) {
		len -= i;
		str += i;
	}

	/* find the end of request line */
	lf = memchr(str, '\n', len);
	if (!lf)
		return (PLM_HTTP_PARSE_AGAIN);

	len = lf - 1 - str;
	*(lf - 1) = '\0';
	
	/* split method, url, version */
	p = memchr(str, ' ', len);
	if (!p)
		return (PLM_HTTP_PARSE_ERROR);

	/* parse method */
	*p = '\0';
	switch (*str) {
	case 'C':
		if (0 == strcmp(str, "CONNECT"))
			mthd = PLM_MTHD_CONNECT;
		break;
	case 'D':
		if (0 == strcmp(str, "DELETE"))
			mthd = PLM_MTHD_DELETE;
		break;
	case 'G':
		if (0 == strcmp(str, "GET"))
			mthd = PLM_MTHD_GET;
		break;
	case 'H':
		if (0 == strcmp(str, "HEAD"))
			mthd = PLM_MTHD_HEAD;
		break;
	case 'P':
		if (0 == strcmp(str, "POST"))
			mthd = PLM_MTHD_POST;
		else if (0 == strcmp(str, "PUT"))
			mthd = PLM_MTHD_PUT;
		break;
	case 'O':
		if (0 == strcmp(str, "OPTIONS"))
			mthd = PLM_MTHD_OPTIONS;
		break;
	case 'T':
		if (0 == strcmp(str, "TRACE"))
			mthd = PLM_MTHD_TRACE;
		break;
	}

	if (mthd == PLM_MTHD_NONE)
		return (PLM_HTTP_PARSE_ERROR);

	/* find URL */
	while (*(++p) == ' ');
	len -= p - str;
	str = p;
	p = memchr(str, ' ', len);
	if (!p)
		return (PLM_HTTP_PARSE_ERROR);

	*p = '\0';
	url.s_str = str;
	url.s_len = p - str;

	/* parse HTTP VERSION */
	while (*(++p) == ' ');
	len -= p - str;
	verstr.s_str = p;
	verstr.s_len = len;

	if (memcmp(verstr.s_str, "HTTP/", 5))
		return (PLM_HTTP_PARSE_ERROR);

	do {
		char *maj, *min;
		int v1, v2;

		min = maj = verstr.s_str + 5;
		while (*min) {
			if (*min == '.') {
				*min++ = '\0';
				break;
			}
			min++;
		}

		/* min reach the end means that version string is invalid */
		if (*min == '\0')
			return (PLM_HTTP_PARSE_ERROR);
		
		v1 = (uint8_t)atoi(maj);
		v2 = (uint8_t)atoi(min);

		if (v1 == 1) {
			if (v2 == 1)
				ver = PLM_HTTP_11;
			else if (v2 == 0)
				ver = PLM_HTTP_10;
		} else if (v1 == 0 && v2 == 9) {
			ver = PLM_HTTP_09;
		}

	} while (0);

	assert(psr->hp_on_req_line);
	psr->hp_parsed = lf + 1 - s->s_str;
	
	assert(psr->hp_parsed <= s->s_len);
	s->s_str += psr->hp_parsed;
	s->s_len -= psr->hp_parsed;

	psr->hp_state = PLM_PRS_RL_DONE;
	return (psr->hp_on_req_line(mthd, &url, ver, psr->hp_data) ?
			PLM_HTTP_PARSE_BREAK : PLM_HTTP_PARSE_DONE);
}

static int
plm_http_parser_field(plm_http_parser_t *psr, plm_string_t *s)
{
	int rc = PLM_HTTP_PARSE_AGAIN;
	
	for (;;) {
		char *str = s->s_str, *lf, *p;
		size_t len = s->s_len, i;
		plm_string_t key, value;

		for (i = 0; i < len && str[i] == ' '; i++) /* none */ ;
		str += i;
		len -= i;

		lf = memchr(str, '\n', len);
		if (!lf)
			return (PLM_HTTP_PARSE_AGAIN);

		if (*(lf - 1) != '\r')
			return (PLM_HTTP_PARSE_ERROR);

		len = lf - 1 - str;
		*(lf - 1) = '\0';

		p = strchr(str, ':');
		if (!p) {
			rc = PLM_HTTP_PARSE_ERROR;
			break;
		}

		*p = '\0';
		for (i = p - str - 1; i >= 0 && i == ' '; i--) /* none */ ;

		key.s_str = str;
		key.s_len = i + 1;

		while (*(++p) == ' ') /* none */ ;
		value.s_str = p;
		value.s_len = len - (p - str);

		psr->hp_parsed += lf + 1 - s->s_str;
		if (psr->hp_on_field) {
			if (psr->hp_on_field(&key, &value, psr->hp_data)) {
				rc = PLM_HTTP_PARSE_BREAK;
				break;
			}
		}

		s->s_len -= psr->hp_parsed;
		s->s_str += psr->hp_parsed;
		if (s->s_len <= 0)
			break;

		if (s->s_str[0] == '\r') {
			psr->hp_parsed++;
			psr->hp_state = PLM_PRS_HDR_CR;
			if (s->s_len - 1 > 0 && s->s_str[1] == '\n') {
				rc = PLM_HTTP_PARSE_DONE;
				psr->hp_state = PLM_PRS_HDR_DONE;
				psr->hp_parsed++;

				if (psr->hp_on_hdr_done)
					psr->hp_on_hdr_done(psr->hp_data);
			}
			break;
		}
	}

	return (rc);
}

static int
plm_http_parser_status_line(plm_http_parser_t *psr, plm_string_t *s)
{
	char *str = s->s_str, *lf, *p;
	size_t len = s->s_len, i;
	plm_string_t verstr, desc;
	enum plm_http_ver ver = PLM_HTTP_VNONE;
	int code;

	for (i = 0; i < len && str[i] == ' '; i++) /* none */;

	if (i > 0) {
		len -= i;
		str += i;
	}

	/* find the end of status line */
	lf = memchr(str, '\n', len);
	if (!lf)
		return (PLM_HTTP_PARSE_AGAIN);
	
	if (*(lf - 1) != '\r')
		return (PLM_HTTP_PARSE_ERROR);

	len = lf - 1 - str;
	*(lf - 1) = '\0';
	
	/* split version, code, description */
	p = memchr(str, ' ', len);
	if (!p)
		return (PLM_HTTP_PARSE_ERROR);

	*p = '\0';

	/* HTTP VERSION */
	verstr.s_str = str;
	verstr.s_len = p - str;
	if (memcmp(verstr.s_str, "HTTP/", 5))
		return (PLM_HTTP_PARSE_ERROR);

	do {
		char *maj, *min;
		int v1, v2;

		min = maj = verstr.s_str + 5;
		while (*min) {
			if (*min == '.') {
				*min++ = '\0';
				break;
			}
			min++;
		}

		/* min reach the end means that version string is invalid */
		if (*min == '\0')
			return (PLM_HTTP_PARSE_ERROR);
		
		v1 = (uint8_t)atoi(maj);
		v2 = (uint8_t)atoi(min);

		if (v1 == 1) {
			if (v2 == 1)
				ver = PLM_HTTP_11;
			else if (v2 == 0)
				ver = PLM_HTTP_10;
		} else if (v1 == 0 && v2 == 9) {
			ver = PLM_HTTP_09;
		}

	} while (0);

	/* http response code */
	while (*(++p) == ' ');
	len -= p - str;	
	str = p;
	p = memchr(str, ' ', len);
	if (!p)
		return (PLM_HTTP_PARSE_ERROR);
	
	*p = '\0';
	code = atoi(str);

	/* description */
	while (*(++p) == ' ');
	len -= p - str;
	str = p;
	desc.s_str = str;
	desc.s_len = len;

	psr->hp_parsed = lf + 1 - s->s_str;
	assert(psr->hp_parsed <= s->s_len);
	s->s_str += psr->hp_parsed;
	s->s_len -= psr->hp_parsed;
	
	assert(psr->hp_on_status_line);
	psr->hp_state = PLM_PRS_SL_DONE;
	return (psr->hp_on_status_line(ver, code, &desc, psr->hp_data) ?
			PLM_HTTP_PARSE_BREAK : PLM_HTTP_PARSE_DONE);
}

int plm_http_parser_req(plm_http_parser_t *psr, plm_string_t *s)
{
	int rc;

	psr->hp_parsed = 0;

RETRY:
	switch (psr->hp_state) {
	case PLM_PRS_NONE:
		rc = plm_http_parser_req_line(psr, s);
		if (PLM_HTTP_PARSE_DONE == rc) {
			if (s->s_len > 0)
				goto RETRY;
			else
				rc = PLM_HTTP_PARSE_AGAIN;
		}
		break;
		
	case PLM_PRS_RL_CR:
		psr->hp_parsed++;		
		if (s->s_str[0] == '\n') {
			psr->hp_state = PLM_PRS_RL_DONE;
			s->s_str++;
			s->s_len--;
			if (s->s_len > 0)
				goto RETRY;
			else
				rc = PLM_HTTP_PARSE_AGAIN;
		} else {
			rc = PLM_HTTP_PARSE_ERROR;
		}
		break;

	case PLM_PRS_RL_DONE:
		rc = plm_http_parser_field(psr, s);
		break;

	case PLM_PRS_HDR_CR:
		psr->hp_parsed++;
		if (s->s_str[0] == '\n') {
			psr->hp_state = PLM_PRS_HDR_DONE;
			s->s_str++;
			s->s_len--;
			rc = PLM_HTTP_PARSE_DONE;
		} else {
			rc = PLM_HTTP_PARSE_ERROR;
		}
		break;

	case PLM_PRS_HDR_DONE:
		rc = PLM_HTTP_PARSE_DONE;
		break;

	default:
		rc = PLM_HTTP_PARSE_ERROR;
		break;
	}

	return (rc);
}

int plm_http_parser_resp(plm_http_parser_t *psr, plm_string_t *s)
{
	int rc;

	psr->hp_parsed = 0;

RETRY:
	switch (psr->hp_state) {
	case PLM_PRS_NONE:
		rc = plm_http_parser_status_line(psr, s);
		if (PLM_HTTP_PARSE_DONE == rc) {
			if (s->s_len > 0)
				goto RETRY;
			else
				rc = PLM_HTTP_PARSE_AGAIN;
		}
		break;

	case PLM_PRS_SL_CR:
		psr->hp_parsed++;
		if (s->s_str[0] == '\n') {
			psr->hp_state = PLM_PRS_SL_DONE;
			s->s_str++;
			s->s_len--;
			if (s->s_len > 0)
				goto RETRY;
			else
				rc = PLM_HTTP_PARSE_AGAIN;
		} else {
			rc = PLM_HTTP_PARSE_ERROR;
		}
		break;
		
	case PLM_PRS_SL_DONE:
		rc = plm_http_parser_field(psr, s);
		break;

	case PLM_PRS_HDR_CR:
		psr->hp_parsed++;
		if (s->s_str[0] == '\n') {
			psr->hp_state = PLM_PRS_HDR_DONE;
			s->s_str++;
			s->s_len--;
			rc = PLM_HTTP_PARSE_DONE;
		} else {
			rc = PLM_HTTP_PARSE_ERROR;
		}
		break;		

	case PLM_PRS_HDR_DONE:
		rc = PLM_HTTP_PARSE_DONE;
		break;

	default:
		rc = PLM_HTTP_PARSE_ERROR;
		break;
	}

	return (rc);
}

int plm_http_parser_url(struct plm_http_url *out, const plm_string_t *url)
{
	int state;
	size_t len;
	char *p1, *p2;

	enum plm_url_state {
		PLM_URL_NONE,
		PLM_URL_SCHEME,
		PLM_URL_HOST,
		PLM_URL_PORT,
		PLM_URL_PATH
	};

	len = url->s_len;
	p1 = url->s_str;
	state = PLM_URL_SCHEME;

	memset(out, 0, sizeof(*out));

	do {
		switch (state) {
		case PLM_URL_SCHEME:
			p2 = memchr(p1, ':', len);
			if (!p2) {
				state = PLM_URL_PATH;
				continue;
			}
			
			out->hu_scheme.s_str = p1;
			out->hu_scheme.s_len = p2 - p1;
			state = PLM_URL_HOST;
			break;
			
		case PLM_URL_HOST:
			if (memcmp(p1, "//", 2))
				return (PLM_HTTP_PARSE_ERROR);

			p1 += 2;
			len -= 2;
			p2 = memchr(p1, ':', len);
			if (!p2) {
				p2 = memchr(p1, '/', len);
				if (!p2)
					return (PLM_HTTP_PARSE_ERROR);
				
				state = PLM_URL_PATH;
			} else {
				state = PLM_URL_PORT;
			}
			
			out->hu_host.s_str = p1;
			out->hu_host.s_len = p2 - p1;
			break;
			
		case PLM_URL_PORT:
			p2 = memchr(p1, '/', len);
			if (!p2)
				return (PLM_HTTP_PARSE_ERROR);
			
			out->hu_port.s_str = p1;
			out->hu_port.s_len = p2 - p1;
			state = PLM_URL_PATH;
			break;
			
		case PLM_URL_PATH:
			out->hu_path.s_str = p1;
			out->hu_path.s_len = len;

			/* break while */
			p2 = p1 + len - 1;
			break;
		}

		len -= ++p2 - p1;
		p1 = p2;
	} while (len > 0);

	return (PLM_HTTP_PARSE_DONE);
}
