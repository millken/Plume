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

#include <stdlib.h>
#include <string.h>
#include "plm_string.h"

int plm_strdup(plm_string_t *out, const plm_string_t *in)
{
	int rc = -1;
	
	out->s_str = (char *)malloc(in->s_len);
	if (out->s_str) {
		memcpy(out->s_str, in->s_str, in->s_len);
		out->s_len = in->s_len;
		rc = 0;
	}
	
	return (rc);
}

int plm_strcmp(const plm_string_t *s1, const plm_string_t *s2)
{
	int rc;

	if (s1->s_len == s2->s_len)
		rc = memcmp(s1->s_str, s2->s_str, s1->s_len);
	else if (s1->s_len - s2->s_len == 1 && s1->s_str[s1->s_len - 1] == 0)
		rc = memcmp(s1->s_str, s2->s_str, s2->s_len);
	else if (s2->s_len - s1->s_len == 1 && s2->s_str[s2->s_len - 1] == 0)
		rc = memcmp(s1->s_str, s2->s_str, s1->s_len);
	else
		rc = s1->s_len > s2->s_len ? 1 : -1;

	return (rc);
}

static int plm_casecmp(const char *s1, const char *s2, size_t len)
{
	static int dv = 'a' - 'A';

#define ISCHAR(c) (c <= 'z' && c >= 'A')	

#define CASECMP(c1, c2)								\
	(ISCHAR(c1) && ISCHAR(c2) ?						\
		(c1 == c2 || c1 - c2 == dv || c2 - c1 == dv) : c1 == c2)

	size_t i;

	for (i = 0; i < len; i++) {
		if (CASECMP(s1[i], s2[i]))
			break;
	}

	return (i == len ? 0 : s1[i] - s2[i]);
}

int plm_strcasecmp(const plm_string_t *s1, const plm_string_t *s2)
{
	int rc;
	
	if (s1->s_len == s2->s_len)
		rc = plm_casecmp(s1->s_str, s2->s_str, s1->s_len);
	else if (s1->s_len - s2->s_len == 1 && s1->s_str[s1->s_len - 1] == 0)
		rc = plm_casecmp(s1->s_str, s2->s_str, s2->s_len);
	else if (s2->s_len - s1->s_len == 1 && s2->s_str[s2->s_len - 1] == 0)
		rc = plm_casecmp(s1->s_str, s2->s_str, s1->s_len);
	else
		rc = s1->s_len > s2->s_len ? 1 : -1;

	return (rc);
}

int plm_strcat2(plm_string_t *out, const plm_string_t *dup,
				const plm_string_t *cat)
{
	int len;
	char *str;

	len = dup->s_len + cat->s_len;
	str = (char *)malloc(len * sizeof(char));
	if (str) {
		out->s_str = str;
		out->s_len = len;
		memcpy(out->s_str, dup->s_str, dup->s_len);
		memcpy(out->s_str + dup->s_len, cat->s_str, cat->s_len);
	}

	return (str ? len : -1);
}

#define STR2SI()								\
	int sign = 1, beg = 0;						\
	if (len > 0 && num[0] == '-') {				\
		sign = -1;								\
		beg++;									\
	}											\
	for (i = beg; i < len; i++) {				\
		if (num[i] < '0' || num[i] > '9')		\
			break;								\
		value = value * 10 + (num[i] - '0');	\
	}											\
	if (sign == -1) value *= -1;

char plm_str2c(const plm_string_t *str)
{
	char value = 0;
	int len = str->s_len, i;
	char *num = str->s_str;

	STR2SI();
	return (value);
}

short plm_str2s(const plm_string_t *str)
{
	short value = 0;
	int len = str->s_len, i;
	char *num = str->s_str;

	STR2SI();
	return (value);	
}

int plm_str2i(const plm_string_t *str)
{
	int value = 0;
	int len = str->s_len, i;
	char *num = str->s_str;

	STR2SI();
	return (value);	
}

long plm_str2l(const plm_string_t *str)
{
	long value = 0;
	int len = str->s_len, i;
	char *num = str->s_str;

	STR2SI();
	return (value);		
}

long long plm_str2ll(const plm_string_t *str)
{
	long long value = 0;
	int len = str->s_len, i;
	char *num = str->s_str;

	STR2SI();
	return (value);			
}

