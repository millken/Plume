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

#ifndef _PLM_HTTP_ERRLOG_H
#define _PLM_HTTP_ERRLOG_H

#include "plm_log.h"

#ifdef __cplusplus
#endif

extern int plm_http_log_level;

#define DEBUG(fmt, ...)						 \
	if (plm_http_log_level >= PLM_LOG_DEBUG) \
		plm_log_write(PLM_LOG_DEBUG, "%s: "fmt, __FUNCTION__, __VA_ARGS__)

#define TRACE(fmt, ...)						 \
	if (plm_http_log_level >= PLM_LOG_TRACE) \
		plm_log_write(PLM_LOG_TRACE, "%s: "fmt, __FUNCTION__, __VA_ARGS__)

#define FATAL(fmt, ...)						 \
	if (plm_http_log_level >= PLM_LOG_FATAL) \
		plm_log_write(PLM_LOG_FATAL, "%s: "fmt, __FUNCTION__, __VA_ARGS__)

#ifdef __cplusplus
#endif

#endif

