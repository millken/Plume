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

#include "plm_plugin.h"
#include "plm_event.h"
#include "plm_lookaside_list.h"
#include "plm_http_request.h"
#include "plm_http_plugin.h"

#define DEF_BACKLOG 5

static void *plm_http_ctx_create(void *);
static void plm_http_ctx_destroy(void *);
static int plm_http_port_set(void *, plm_dlist_t *);
static int plm_http_listen_set(void *, plm_dlist_t *);

struct plm_plugin http_plugin;
static struct plm_cmd http_cmds[] = {
	{
		&http_plugin,
		plm_string("http"),
		PLM_BLOCK,
		NULL,
		plm_http_ctx_create,
		plm_http_ctx_destroy
	},
	{
		&http_plugin,
		plm_string("http_port"),
		PLM_INSTRUCTION,
		plm_http_port_set,
		NULL,
		NULL
	},
	{
		&http_plugin,
		plm_string("http_listen"),
		PLM_INSTRUCTION,
		plm_http_listen_set,
		NULL,
		NULL
	},
	{0}
};

static void plm_http_set_main_conf(struct plm_share_param *);
static int plm_http_on_work_proc_start(struct plm_ctx_list *);
static void plm_http_on_work_proc_exit(struct plm_ctx_list *);
static int plm_http_on_work_thrd_start(struct plm_ctx_list *);
static void plm_http_on_work_thrd_exit(struct plm_ctx_list *);

struct plm_plugin http_plugin = {
	plm_http_set_main_conf,
	plm_http_on_work_proc_start,
	plm_http_on_work_proc_exit,
	plm_http_on_work_thrd_start,
	plm_http_on_work_thrd_exit,
	echo_cmds
};

void *plm_http_ctx_create(void *parent)
{
	struct plm_http_ctx *ctx;

	ctx = (struct plm_http_ctx *)malloc(sizeof(struct plm_http_ctx));
	if (ctx) {
		memset(ctx, 0, sizeof(struct plm_http_ctx));
		PLM_DLIST_INIT(&ctx->hc_sub_block);
		ctx->hc_backlog = DEF_BACKLOG;
	}
	return (ctx);
}

void plm_http_ctx_destroy(void *ctx)
{
	free(ctx);
}

int plm_http_port_set(void *data, plm_dlist_t *param_list)
{
	struct plm_http_ctx *ctx;
	struct plm_cmd_param *param;

	ctx = (struct plm_http_ctx *)data;
	if (PLM_DLIST_LEN(param_list) != 1) {
		plm_log_syslog("the number of http_port param is wrong");
		return (-1);
	}

	param = (struct plm_cmd_param *)PLM_DLIST_FRONT(param_list);
	ctx->hc_port = plm_str2s(&param->cp_data);
	return (0);
}

int plm_http_listen_set(void *ctx, plm_dlist_t *param_list)
{
	int n;
	struct plm_http_ctx *ctx;
	struct plm_cmd_param *param;

	n = PLM_DLIST_LEN(param_list);
	ctx = (struct plm_http_ctx *)data;
	if (n != 1 || n != 2) {
		plm_log_syslog("the number of http_listen param is wrong");
		return (-1);
	}

	param = (struct plm_http_param *)PLM_DLIST_FRONT(param_list);
	plm_strdup(&ctx->hc_addr, &param->cp_data);
	if (!ctx->hc_addr.s_str) {
		plm_log_syslog("strdup failed, memory emergent");
		return (-1);
	}

	if (n == 2) {
		param = (struct plm_http_param *)PLM_DLIST_NEXT(&param->cp_node);
		ctx->hc_backlog = plm_str2i(&param->cp_data);
	}

	return (0);
}

static struct plm_share_param sp;

void plm_http_set_main_conf(struct plm_share_param *param)
{
	memcpy(&sp, param, sizeof(sp));
}

int plm_http_on_work_proc_start(struct plm_ctx_list *cl)
{
	int err = -1;
	struct plm_http_ctx *ctx;	

	ctx = (struct plm_http_ctx *)PLM_CTX_LIST_GET_POINTER(cl);
	plm_lookaside_list_init(&ctx->hc_conn_pool, &sp.sp_maxfd,
							sizeof(struct plm_http_conn), sp.sp_tag,
							malloc, free);
	plm_lookaside_list_enable(&ctx->conn_pool,
							  sp.sp_zeromem, sp.sp_tagcheck, sp.sp_thrdn > 1);
	ctx->hc_fd = plm_comm_open(PLM_COMM_TCP, NULL, 0, 0, ctx->hc_port,
							   ctx->hc_addr.s_str, ctx->hc_backlog, 1);
	if (ctx->hc_fd < 0) {
		plm_log_syslog("can't open http plugin listen fd: %s:%d",
					   ctx->hc_addr._str, ctx->hc_port);
	} else {
		err = plm_event_io_read(ctx->hc_fd, ctx, plm_http_accept);
		if (err)
			plm_log_syslog("plm_event_io_read failed on listen fd");
	}
	
	return (err);
}

void plm_http_on_work_proc_exit(struct plm_ctx_list *cl)
{	
	struct plm_http_ctx *ctx;
	ctx = (struct plm_http_ctx *)PLM_CTX_LIST_GET_POINTER(cl);
	plm_comm_close(ctx->hc_fd);
}

int plm_http_on_work_thrd_start(struct plm_ctx_list *cl)
{
	return (0);
}

void plm_http_on_work_thrd_exit(struct plm_ctx_list *cl)
{	
}




