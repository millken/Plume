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

#ifndef _PLM_STRING_H
#define _PLM_STRING_H

#include <string.h>
#include "plm_mempool.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct plm_string {
	char *s_str;
	int s_len;
} plm_string_t;

#define plm_string_null { NULL, 0 };
#define plm_string(s) { s, sizeof(s) }

#define plm_strlen(s) (s)->s_len
#define plm_strappend(s, a, n, p) \
	do { \
		const char *old_mem = (s)->s_str; \
		size_t new_len = (s)->s_len + n; \
		char *new_mem = (char *)plm_mempool_alloc(p, new_len); \
		if (new_mem) { \
			memcpy(new_mem, old_mem, (s)->s_len); \
			memcpy(new_mem+(s)->s_len, a, n); \
			(s)->s_str = new_mem; \
			(s)->s_len = new_len; \
		} \
	} while(0)
#define plm_strassign(s, a, n, p) \
	do { \
		char *str = (char *)plm_mempool_alloc(p, n); \
		if (str) { \
			memcpy(str, a, n); \
			(s)->s_str = str; \
			(s)->s_len = n; \
		} \
	} while(0)

int plm_strdup(plm_string_t *out, plm_string_t *in);
int plm_strcmp(plm_string_t *s1, plm_string_t *s2);
int plm_strcat2(plm_string_t *out, const plm_string_t *dup,
				const plm_string_t *cat);
	
char plm_str2c(plm_string_t *str);
short plm_str2s(plm_string_t *str);
int plm_str2i(plm_string_t *str);
long plm_str2l(plm_string_t *str);

#ifdef __cplusplus
}
#endif

#endif

