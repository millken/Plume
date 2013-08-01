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

#include <dlfcn.h>
#include <signal.h>

#include "plm_list.h"
#include "plm_log.h"
#include "plm_ctx.h"
#include "plm_comm.h"
#include "plm_threads.h"
#include "plm_timer.h"
#include "plm_plugin_base.h"

static int plm_logpath_set(void *, plm_dlist_t *);
static int plm_work_thread_num_set(void *, plm_dlist_t *);
static int plm_load_plugin_set(void *, plm_dlist_t *);
static int plm_maxfd_set(void *, plm_dlist_t *);
static int plm_work_thread_cpu_affinity_set(void *, plm_dlist_t *);
static int plm_memtag_set(void *, plm_dlist_t *);
static int plm_tagcheck_set(void *, plm_dlist_t *);
static int plm_zeromem_set(void *, plm_dlist_t *);

static void *plm_main_ctx_create(void *unused);
static void plm_main_ctx_destroy(void *ctx);

struct plm_main_ctx main_ctx;
static struct plm_plugin main_plugin;

static struct plm_cmd main_cmd[] = {
	{
		&main_plugin,
		plm_string("main"),
		PLM_BLOCK,
		NULL,
		plm_main_ctx_create,
		plm_main_ctx_destroy
	},
	{
		&main_plugin,
		plm_string("work_thread_num"),
		PLM_INSTRUCTION,
		plm_work_thread_num_set,
		NULL,
		NULL		
	},
	{
		&main_plugin,
		plm_string("logpath"),
		PLM_INSTRUCTION,
		plm_logpath_set,
		NULL,
		NULL
	},
	{
		&main_plugin,
		plm_string("load_plugin"),
		PLM_INSTRUCTION,
		plm_load_plugin_set,
		NULL,
		NULL
	},
	{
		&main_plugin,
		plm_string("maxfd"),
		PLM_INSTRUCTION,
		plm_maxfd_set,
		NULL,
		NULL
	},
	{
		&main_plugin,
		plm_string("work_thread_cpu_affinity"),
		PLM_INSTRUCTION,
		plm_work_thread_cpu_affinity_set,
		NULL,
		NULL
	},
	{
		&main_plugin,
		plm_string("memtag"),
		PLM_INSTRUCTION,
		plm_memtag_set,
		NULL,
		NULL
	},
	{
		&main_plugin,
		plm_string("tagcheck"),
		PLM_INSTRUCTION,
		plm_tagcheck_set,
		NULL,
		NULL
	},
	{
		&main_plugin,
		plm_string("zeromem"),
		PLM_INSTRUCTION,
		plm_zeromem_set,
		NULL,
		NULL
	},
	{0}
};

static int plm_main_on_work_proc_start(struct plm_ctx_list *);
static void plm_main_on_work_proc_exit(struct plm_ctx_list *);

static struct plm_plugin main_plugin = {
	NULL,
	plm_main_on_work_proc_start,
	plm_main_on_work_proc_exit,
	NULL,
	NULL,
	main_cmd
};

struct plm_plugin_node {
	plm_dlist_node_t pn_node;
	struct plm_plugin *pn_plg;
};

static struct plm_share_param sp;
static plm_dlist_t plugins;
static struct plm_plugin *core_plg[] = {
	&main_plugin
};

static void plm_plugin_work_proc_init_eachone(void *n, void *data)
{
	int *rc;
	struct plm_ctx_list *plg_ctx;

	plg_ctx = (struct plm_ctx_list *)n;
	rc = (int *)data;

	if (plg_ctx->cl_plg->plg_set_conf)
		plg_ctx->cl_plg->plg_set_conf(&sp);
	
	*rc |= plg_ctx->cl_plg->plg_on_work_proc_start(plg_ctx);
}

/* init all plugins */
int plm_plugin_init()
{
	int i;
	int rc = 0;
	plm_list_t *list = &main_ctx.mc_ctxs;
	int thrdn = main_ctx.mc_work_thread_num;

	sp.sp_thrdn = main_ctx.mc_work_thread_num;
	sp.sp_maxfd = main_ctx.mc_maxfd;
	sp.sp_tagcheck = main_ctx.mc_tagcheck;
	sp.sp_zeromem = main_ctx.mc_zeromem;
	sp.sp_tag = main_ctx.mc_tag;

	/* init core plugins */
	for (i = 0; i < sizeof(core_plg) / sizeof(core_plg[0]); i++) {
		if (core_plg[i]->plg_set_conf)
			core_plg[i]->plg_set_conf(&sp);
		
		if (core_plg[i]->plg_on_work_proc_start) {
			rc = core_plg[i]->plg_on_work_proc_start(NULL);
			if (rc)
				return (-2);
		}
	}
	
	PLM_LIST_FOREACH(list, plm_plugin_work_proc_init_eachone, &rc);

	/* start work thread if needed */
	if (!rc) {
		/* create work thread */
		plm_disp_start();
	}
	
	return (rc ? -2 : 0);
}

static void plm_plugin_work_proc_destroy_eachone(void *n, void *data)
{
	struct plm_ctx_list *plg_ctx;

	plg_ctx = (struct plm_ctx_list *)n;
	plg_ctx->cl_plg->plg_on_work_proc_exit(plg_ctx);
}

/* destroy all plugins */
void plm_plugin_destroy()
{
	int i;
	plm_list_t *list;

	list = &main_ctx.mc_ctxs;
	PLM_LIST_FOREACH(list, plm_plugin_work_proc_destroy_eachone, NULL);

	for (i = 0; i < sizeof(core_plg) / sizeof(core_plg[0]); i++) {
		if (core_plg[i]->plg_on_work_proc_exit)
			core_plg[i]->plg_on_work_proc_exit(NULL);
	}
	
	plm_disp_shutdown();
}

struct plm_cmd_search {
	struct plm_cmd **cs_pp;
	plm_string_t *cs_name;
};

int plm_plugin_search(plm_dlist_node_t *n, void *data)
{
	int i;
	struct plm_plugin_node *pn;
	struct plm_cmd *cmd;
	struct plm_cmd cmd_null = {0};
	struct plm_cmd_search *cs;

	pn = (struct plm_plugin_node *)n;
	cs = (struct plm_cmd_search *)data;

	cmd = pn->pn_plg->plg_cmds;
	for (i = 0; memcmp(&cmd_null, &cmd[i], sizeof(cmd_null)); i++) {
		if (plm_strcmp(&cmd[i].c_name, cs->cs_name) == 0) {
			*cs->cs_pp = &cmd[i];
			break;
		}
	}

	return (*cs->cs_pp ? 1 : 0);
}

/* return 0 indicate found */
int plm_plugin_cmd_search(struct plm_cmd **pp, plm_string_t *name)
{
	plm_dlist_node_t *node = NULL;
	struct plm_cmd_search cs;
	struct plm_cmd cmd_null = {0};
	struct plm_cmd *main_cmd = main_plugin.plg_cmds;
	int i;

	for (i = 0; memcmp(&cmd_null, &main_cmd[i], sizeof(cmd_null)); i++) {
		if (plm_strcmp(&main_cmd[i].c_name, name) == 0) {
			*pp = &main_cmd[i];
			return (0);
		}
	}

	*pp = NULL;
	cs.cs_name = name;
	cs.cs_pp = pp;
	PLM_DLIST_SEARCH(&node, &plugins, plm_plugin_search, &cs);
	return (*pp ? 0 : -1);
}

int plm_logpath_set(void *ctx, plm_dlist_t *params)
{
	plm_dlist_node_t *node;
	plm_string_t logpath = {0};
	plm_string_t loglevel = {0};

	for (node = PLM_DLIST_FRONT(params); node; node = PLM_DLIST_NEXT(node)) {
		struct plm_cmd_param *param;

		param = (struct plm_cmd_param *)node;
		
		if (logpath.s_len == 0)
			plm_strdup(&logpath, &param->cp_data);
		else if (loglevel.s_len == 0)
			loglevel = param->cp_data;
	}

	if (logpath.s_len > 0) {
		free(main_ctx.mc_log_path.s_str);
		main_ctx.mc_log_path = logpath;
	}

	if (loglevel.s_len > 0)
		main_ctx.mc_log_level = plm_str2i(&loglevel);

	return (0);
}

int plm_work_thread_num_set(void *ctx, plm_dlist_t *params)
{
	plm_dlist_node_t *node;
	struct plm_cmd_param *param;

	node = PLM_DLIST_FRONT(params);
	param = (struct plm_cmd_param *)node;

	main_ctx.mc_work_thread_num = plm_str2i(&param->cp_data);
	return (0);
}

static struct plm_plugin *
plm_load_plugin(const plm_string_t *path, const plm_string_t *syb)
{
	char filepath[MAX_PATH];
	char symbol[MAX_PATH];
	void *handle;
	void *rc = NULL;
	const char *err = NULL;

	memset(filepath, 0, sizeof(filepath));
	memset(symbol, 0, sizeof(symbol));
	memcpy(filepath, path->s_str, path->s_len);
	memcpy(symbol, syb->s_str, syb->s_len);

	/* the handle never close */
	handle = dlopen(filepath, RTLD_NOW);
	if (handle) {
		rc = dlsym(handle, symbol);
		if (!rc)
			err = dlerror();
	}

	return (rc);
}

int plm_load_plugin_set(void *ctx, plm_dlist_t *params)
{
	plm_dlist_node_t *node;
	struct plm_cmd_param *filename, *sybname;
	struct plm_plugin_node *pn;

	node = PLM_DLIST_FRONT(params);
	filename = (struct plm_cmd_param *)node;
	sybname = (struct plm_cmd_param *)PLM_DLIST_NEXT(node);

	pn = (struct plm_plugin_node *)malloc(sizeof(struct plm_plugin_node));
	if (pn) {
		pn->pn_plg = plm_load_plugin(&filename->cp_data, &sybname->cp_data);
		if (pn->pn_plg) {
			PLM_DLIST_ADD_BACK(&plugins, &pn->pn_node);
		} else {
			free(pn);
			pn = NULL;
		}
	}

	return (pn ? 0 : -1);
}

int plm_maxfd_set(void *ctx, plm_dlist_t *params)
{
	plm_dlist_node_t *node;
	struct plm_cmd_param *param;

	node = PLM_DLIST_FRONT(params);
	param = (struct plm_cmd_param *)node;

	main_ctx.mc_maxfd = plm_str2i(&param->cp_data);
	return (0);
}

int64_t plm_bitset2int64(plm_string_t *s)
{
	int n = s->s_len - 1, i;
	char *str = s->s_str;
	int64_t v = 0;
	
	for (i = n; i >= 0; i--) {
		if (str[i] == '1')
			v |= 1 << (n - i);
	}

	return (v);
}

int plm_work_thread_cpu_affinity_set(void *ctx, plm_dlist_t *params)
{
	int8_t i = 0, n;
	struct plm_cmd_param *param;

	n = PLM_DLIST_LEN(params);
	if (n == 0)
		return (-1);

	if (n < 0)
		n = 127;

	main_ctx.mc_cpu_affinity_id = (uint64_t *)malloc(n * sizeof(uint64_t));
	if (!main_ctx.mc_cpu_affinity_id)
		return (-1);

	main_ctx.mc_cpu_affinity_id_num = n;
	param = (struct plm_cmd_param *)PLM_DLIST_FRONT(params);
	while (param) {
		main_ctx.mc_cpu_affinity_id[i++] = plm_bitset2int64(&param->cp_data);
		param = (struct plm_cmd_param *)PLM_DLIST_NEXT(&param->cp_node);
	}
	
	return (0);
}

unsigned int plm_hex2i(int *err, plm_string_t *str)
{
	
}

int plm_memtag_set(void *ctx, plm_dlist_t *params)
{
	int err;
	struct plm_cmd_param *param;

	if (PLM_DLIST_LEN(params) != 1)
		return (-1);

	param = (struct plm_cmd_param *)PLM_DLIST_FRONT(params);
	main_ctx.mc_tag = plm_hex2i(&err, &param->cp_data);
	return (err);
}

int plm_tagcheck_set(void *ctx, plm_dlist_t *params)
{
	struct plm_cmd_param *param;
	plm_string_t on = plm_string("on");
	
	if (PLM_DLIST_LEN(params) != 1)
		return (-1);

	param = (struct plm_cmd_param *)PLM_DLIST_FRONT(params);
	if (0 == plm_strcmp(&param->cp_data, &on))
		main_ctx.mc_tagcheck = 1;
	else
		main_ctx.mc_tagcheck = 0;

	return (0);
}

int plm_zeromem_set(void *ctx, plm_dlist_t *params)
{
	struct plm_cmd_param *param;
	plm_string_t on = plm_string("on");	

	if (PLM_DLIST_LEN(params) != 1)
		return (-1);

	param = (struct plm_cmd_param *)PLM_DLIST_FRONT(params);
	if (0 == plm_strcmp(&param->cp_data, &on))
		main_ctx.mc_zeromem = 1;
	else
		main_ctx.mc_zeromem = 0;

	return (0);
}

void *plm_main_ctx_create(void *unused)
{
	extern plm_string_t plm_prefix;
	plm_string_t logs = plm_string("logs/");
	
	main_ctx.mc_work_thread_num = 1;
	main_ctx.mc_maxfd = 1024;
	main_ctx.mc_log_level = 1;
	main_ctx.mc_cpu_affinity_id = NULL;
	main_ctx.mc_cpu_affinity_id_num = 0;
	main_ctx.mc_zeromem = 1;
	main_ctx.mc_tagcheck = 1;
	main_ctx.mc_tag = -1;
	plm_strcat2(&main_ctx.mc_log_path, &plm_prefix, &logs);
	
	return &main_ctx;
}

void plm_main_ctx_destroy(void *unused)
{
	/* destroy main_ctx */
	if (main_ctx.mc_log_path.s_str)
		free(main_ctx.mc_log_path.s_str);
	if (main_ctx.mc_cpu_affinity_id)
		free(main_ctx.mc_cpu_affinity_id);
}

int plm_main_on_work_proc_start(struct plm_ctx_list *ctx)
{
	int maxfd = main_ctx.mc_maxfd;
	int thrdn = main_ctx.mc_work_thread_num;
	int thrdsafe = thrdn > 1;

	plm_buffer_init(thrdsafe);
	if (plm_timer_init(thrdn))
		return (-1);
	
	if (!plm_comm_init(maxfd)) {
		if (!plm_event_io_init(maxfd, thrdn))
			return (0);

		plm_comm_destroy();
	}
	
	plm_timer_destroy();
	plm_buffer_destroy();
	return (-1);
}

void plm_main_on_work_proc_exit(struct plm_ctx_list *ctx)
{
	plm_event_io_shutdown();
	plm_comm_destroy();
	plm_buffer_destroy();
}

static void plm_plugin_work_thrd_init_eachone(void *n, void *data)
{
	struct plm_plugin *plg;
	int *rc = (int *)data;
	struct plm_ctx_list *ctx = (struct plm_ctx_list *)n;

	plg = ctx->cl_plg;
	if (plg->plg_on_work_thrd_start)
		*rc |= plg->plg_on_work_thrd_start(ctx);
}

int plm_plugin_get_cpu_affinity_id()
{
	int i = plm_threads_curr() % main_ctx.mc_cpu_affinity_id_num;
	return main_ctx.mc_cpu_affinity_id[i];
}

int plm_plugin_work_thrd_init()
{
	int rc = 0, id;
	plm_list_t *list = &main_ctx.mc_ctxs;

	if (main_ctx.mc_cpu_affinity_id) {
		id = plm_plugin_get_cpu_affinity_id();
		if (plm_threads_set_cpu_affinity(id))
			plm_log_write(PLM_LOG_FATAL, "set cpu affinity failed");
	}
	
	PLM_LIST_FOREACH(list, plm_plugin_work_thrd_init_eachone, &rc);
	return (rc);
}

static void plm_plugin_work_thrd_destroy_eachone(void *n, void *data)
{
	struct plm_plugin *plg;
	struct plm_ctx_list *ctx = (struct plm_ctx_list *)n;

	plg = ctx->cl_plg;
	if (plg->plg_on_work_thrd_exit)
		plg->plg_on_work_thrd_exit(ctx);
}

void plm_plugin_work_thrd_destroy()
{
	plm_list_t *list = &main_ctx.mc_ctxs;

	PLM_LIST_FOREACH(list, plm_plugin_work_thrd_destroy_eachone, NULL);
}
