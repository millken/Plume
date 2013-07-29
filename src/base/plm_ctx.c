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
#include <stddef.h>

#include "plm_dispatcher.h"
#include "plm_event.h"
#include "plm_plugin_base.h"
#include "plm_ctx.h"

struct plm_ctx_record_elem {
	plm_stack_node_t cre_node;
	plm_list_t *cre_list;
	void *cre_parent;	
	void *cre_ctx;
};

static plm_stack_t ctx_record;

static int plm_ctx_record_push(void *p, void *c, plm_list_t *l);
static void plm_ctx_record_top(void **p, void **c, plm_list_t **l);
static void plm_ctx_record_pop();
static int plm_ctx_record_empty();

/* register context
 * @plg -- current plugin
 * @parent -- the parent block ctx
 * @ctx -- the current block ctx
 * @ctx_free -- free ctx when shutdown
 * return 0 on success
 */
int plm_ctx_register(struct plm_plugin *plg, void *parent,
					 void *ctx, void (*ctx_free)(void *))
{
	static int inited = 0;
	plm_list_t *list = NULL;
	struct plm_ctx_list *cl;

	if (!inited) {
		PLM_LIST_INIT(&main_ctx.mc_ctxs);
		inited = 1;

		/* main context comes first */
		main_ctx.mc_free = ctx_free;
		return (0);
	}

	if (plm_ctx_record_empty()) {
		list = &main_ctx.mc_ctxs;
		if (plm_ctx_record_push(parent, ctx, list))
			return (-1);
	} else {
		void *p, *c;
		
	FIND_LEVEL:
		plm_ctx_record_top(&p, &c, &list);
		if (parent == p) {
			/* the same level */
			plm_ctx_record_pop();
			if (plm_ctx_record_push(parent, ctx, list))
				return (-1);
		} else if (parent == c) {
			/* sub level */
			cl = (struct plm_ctx_list *)
				((char *)list - offsetof(struct plm_ctx_list, cl_list));
			list = &cl->cl_list;
			if (plm_ctx_record_push(parent, ctx, list))
				return (-1);
		} else {
			/* pop levels until we find the correct level */
			plm_ctx_record_pop();
			goto FIND_LEVEL;
		}
	}

	cl = (struct plm_ctx_list *)malloc(sizeof(struct plm_ctx_list));
	if (!cl)
		return (-1);
		
	PLM_LIST_INIT(&cl->cl_list);
	PLM_LIST_ADD_FRONT(list, &cl->cl_node);
	cl->cl_data.c_data = ctx;
	cl->cl_data.c_free = ctx_free;
	cl->cl_plg = plg;

	return (cl ? 0: -1);
}

static int plm_ctx_list_foreach(void *n, void *noused)
{
	struct plm_ctx_list *cl;

	cl = (struct plm_ctx_list *)
		((char *)n - offsetof(struct plm_ctx_list, cl_node));
	
	PLM_LIST_FOREACH(&cl->cl_list, plm_ctx_list_foreach, NULL);

	for (;;) {
		void *node;
		plm_list_t *list;

		list = &main_ctx.mc_ctxs;
		if (PLM_LIST_LEN(list) <= 0)
			break;

		node = PLM_LIST_FRONT(list);
		PLM_LIST_DEL_FRONT(list);

		free(node);
	}

	cl->cl_data.c_free(cl->cl_data.c_data);
	return (0);
}

/* destroy context */
int plm_ctx_destroy()
{
	plm_list_t *list;

	list = &main_ctx.mc_ctxs;
	PLM_LIST_FOREACH(list, plm_ctx_list_foreach, NULL);

	for (;;) {
		void *node;
		
		if (PLM_LIST_LEN(list) <= 0)
			break;

		node = PLM_LIST_FRONT(list);
		PLM_LIST_DEL_FRONT(list);

		free(node);
	}

	main_ctx.mc_free(&main_ctx);
	return (0);
}

int plm_ctx_record_push(void *p, void *c, plm_list_t *l)
{
	struct plm_ctx_record_elem *e;

	e = (struct plm_ctx_record_elem *)
		malloc(sizeof(struct plm_ctx_record_elem));
	if (e) {
		e->cre_list = l;
		e->cre_parent = p;
		e->cre_ctx = c;

		PLM_STACK_PUSH(&ctx_record, &e->cre_node);
	}

	return (e ? 0 : -1);
}

void plm_ctx_record_top(void **p, void **c, plm_list_t **l)
{
	plm_stack_node_t *n;
	struct plm_ctx_record_elem *e;

	PLM_STACK_TOP(&n, &ctx_record);
	e = (struct plm_ctx_record_elem *)n;
	*p = e->cre_parent;
	*c = e->cre_ctx;
	*l = e->cre_list;
}

void plm_ctx_record_pop()
{
	plm_stack_node_t *n;
	struct plm_ctx_record_elem *e;

	PLM_STACK_TOP(&n, &ctx_record);
	PLM_STACK_POP(&ctx_record);
	e = (struct plm_ctx_record_elem *)n;
	free(e);
}

int plm_ctx_record_empty()
{
	int len;

	PLM_STACK_LEN(&len, &ctx_record);
	return (len <= 0);
}
