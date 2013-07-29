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

#ifndef _PLM_PLUGIN_H
#define _PLM_PLUGIN_H

#include "plm_string.h"
#include "plm_list.h"
#include "plm_dlist.h"

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_PATH 256	

struct plm_plugin;

typedef enum {
	CT_BLOCK,
	CT_INSTRUCTION,
} plm_cmd_type_t;

/* all parameter of directive */	
struct plm_cmd_param {
	plm_dlist_node_t cp_node;
	plm_string_t cp_data;
};

/* plugin context */	
struct plm_ctx {
	void (*c_free)(void *);
	void *c_data;
};

/* plugin context list
 * some plugin may have sub module
 * so that it may have a sub context
 * all sub context linked in cl_list
 */	
struct plm_ctx_list {
	plm_list_node_t cl_node;
	plm_list_t cl_list;
	struct plm_ctx cl_data;
	struct plm_plugin *cl_plg;
};	

#define PLM_CTX_LIST_GET_POINTER(ctx) ((ctx)->cl_data.c_data)
	
/* directive structure */	
struct plm_cmd {
	/* plugin which it belongs to */
	struct plm_plugin *c_plg;
	
	/* static string */
	plm_string_t c_name;
	
	/* block or instruction */
	plm_cmd_type_t c_type;

	/* callback to set cmd
	 * return non-zero indicate error
	 */
	int (*c_set_cmd)(void *ctx, plm_dlist_t *params);

	/* create context for block command */
	void *(*c_create_ctx)(void *parent);

	/* destroy context for block command */
	void (*c_destroy_ctx)(void *ctx);
};

/* plugin structure */	
struct plm_plugin {
	/* called when work process start */
	int (*plg_on_work_proc_start)(struct plm_ctx_list *);

	/* called when work process exit */
	void (*plg_on_work_proc_exit)(struct plm_ctx_list *);

	/* called when work thread start */
	int (*plg_on_work_thrd_start)(struct plm_ctx_list *);

	/* called when work thread exit */
	void (*plg_on_work_thrd_exit)(struct plm_ctx_list *);

	/* command array */
	struct plm_cmd *plg_cmds;
};

#ifdef __cplusplus
}
#endif

#endif
