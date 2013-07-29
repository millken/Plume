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

#ifndef _PLM_PLUGIN_BASE_H
#define _PLM_PLUGIN_BASE_H

#include <stdint.h>
#include "plm_plugin.h"

#ifdef __cplusplus
extern "C" {
#endif

struct plm_main_ctx {
	/* log file directroy */
	plm_string_t mc_log_path;

	/* log level */
	int mc_log_level;

	/* thread number per work process */
	int mc_work_thread_num;

	/* max file descriptor number */
	int mc_maxfd;

	/* array of cpu id if set by work_thread_cpu_affinity */
	uint64_t *mc_cpu_affinity_id;
	int8_t mc_cpu_affinity_id_num;

	/* poll timeout value */
	int mc_poll_timeout;

	/* destroy main ctx */
	void (*mc_free)(void *);

	/* all plugin context */
	plm_list_t mc_ctxs;
};

extern struct plm_main_ctx main_ctx;
	
/* init all plugins */
int plm_plugin_init();

/* destroy all plugins */
void plm_plugin_destroy();

/* return 0 indicate found */
int plm_plugin_cmd_search(struct plm_cmd **pp, plm_string_t *cmd);

int plm_plugin_work_thrd_init();

void plm_plugin_work_thrd_destroy();
	
#ifdef __cplusplus
}
#endif

#endif
