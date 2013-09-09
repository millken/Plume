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

#ifndef _PLM_HTTP_EVENT_IO_H
#define _PLM_HTTP_EVENT_IO_H

#include <stdlib.h>
#include <errno.h>

#include "plm_http_errlog.h"
#include "plm_event.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PLM_EVT_DRV_READ(fd, data, fn)							\
	do {														\
        if (plm_event_io_read(fd, data, fn)) {					\
			PLM_FATAL("plm_event_io_read: %s", strerror(errno));\
			exit(-1);											\
		}														\
	} while (0)

#define PLM_EVT_DRV_WRITE(fd, data, fn)								\
	do {															\
        if (plm_event_io_write(fd, data, fn)) {						\
			PLM_FATAL("plm_event_io_write: %s", strerror(errno));	\
			exit(-1);												\
		}															\
	} while (0)

struct plm_http_wrevt {
	void (*hw_fn)(void *, char *, size_t, int);
	void *hw_data;
	char *hw_buf;
	size_t hw_off;
	size_t hw_len;
};

void plm_http_event_write(int fd, struct plm_http_wrevt *we);	

#ifdef __cplusplus
}
#endif

#endif

