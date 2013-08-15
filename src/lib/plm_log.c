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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>
#include <errno.h>

#include "plm_log.h"

struct plm_log {
	FILE *l_fp;
	int l_level;
};

static __thread struct plm_log log;

/* open log file
 * @level -- log level, PLM_LOG_TRACE etc
 * @filepath -- log filepath
 * return 0 : success, -1 : error
 */
int plm_log_open(int level, const char *filepath)
{
	FILE *fp;

	/* close the previous log file if opened */
	plm_log_close();
	
	fp = fopen(filepath, "w");
	if (fp) {
		log.l_fp = fp;
		log.l_level = level;
	}
	
	return (log.l_fp ? 0 : -1);
}

/* close the log file 
 * return 0 : success, -1 : error
 */
int plm_log_close()
{
	int err = 0;
	FILE *fp = log.l_fp;
	
	if (fp) {
		err = fclose(fp);
		if (err) {
			log.l_fp = NULL;
			log.l_level = PLM_LOG_UNKNOWN;
		}
	}

	return (err ? -1 : 0);
}

/* write log message
 * @fmt -- message
 * return bytes written
 */
int plm_log_write(int level, const char *fmt, ...)
{
	int n;
	va_list ap;
	time_t now;
	struct tm t;
	char date_str[128];
	const char *msg;

	if (!log.l_fp || log.l_level == PLM_LOG_UNKNOWN || level > log.l_level)
		return (0);

	switch (level) {
	case PLM_LOG_TRACE:
		msg = "[TRACE]";
		break;
	case PLM_LOG_DEBUG:
		msg = "[DEBUG]";
		break;
	case PLM_LOG_FATAL:
		msg = "[FATAL]";
		break;
	case PLM_LOG_WARNING:
		msg = "[WARNING]";
		break;
	}

	now = time(NULL);
	if (localtime_r(&now, &t))
		strftime(date_str, sizeof(date_str), "%Y/%m/%d %X", &t);
	else
		date_str[0] = 0;

	n = fprintf(log.l_fp, "%s %s: ", date_str, msg);
	
	va_start(ap, fmt);
	n += vfprintf(log.l_fp, fmt, ap);
	n += fprintf(log.l_fp, "\n");
	va_end(ap);

	fflush(log.l_fp);

	return (n);
}

void plm_log_syslog(const char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vsyslog(LOG_USER | LOG_ERR, fmt, ap);
	va_end(ap);
}
