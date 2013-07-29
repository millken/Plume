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
#include <stddef.h>
#include <ctype.h>
#include <errno.h>

#include "plm_log.h"
#include "plm_mempool.h"
#include "plm_plugin.h"
#include "plm_dlist.h"
#include "plm_stack.h"
#include "plm_conf.h"

typedef enum {
	CPS_NULL,
	CPS_BLOCK_BEG,
	CPS_BLOCK_END,
	CPS_CHAR,
	CPS_SPACE,
	CPS_VAR,
	CPS_ASSIGN,
	CPS_LINE_END,
	CPS_INVALID
} plm_conf_parse_state_t;

struct plm_block_ctx {
	plm_stack_node_t bc_node;
	void *bc_data;
};

struct plm_cmd_line {
	int cl_stat;
	plm_string_t cl_name;
	plm_dlist_t cl_params;
};

struct plm_var_elem {
	plm_stack_node_t ve_node;

	unsigned char ve_is_spec:1;
	
	union {
		struct {
			plm_string_t ve_name;
			plm_string_t ve_value;
		} ve_data;

		int ve_block_num;
	} ve_u;
};

struct plm_var_stack {
	int vs_block_num;
	plm_stack_t vs_stack;
};

static struct plm_var_stack var_stack;

static plm_conf_parse_state_t
plm_conf_parse_line(plm_stack_node_t **n, struct plm_mempool *p,
					char *line, int len);

static void plm_conf_var_block_push(struct plm_mempool *p);
static void plm_conf_var_block_pop();

int plm_conf_load()
{
	static char line[1024];

	FILE *fp;
	int block = 0;
	plm_stack_t ctx_stack;
	struct plm_mempool pool;
	plm_stack_node_t *curr_ctx_node = NULL;
	extern plm_string_t plm_prefix;

#define ASSIGN_BLOCK_CTX(c, n) \
	c = (struct plm_block_ctx *)\
		((char *)n - offsetof(struct plm_block_ctx, bc_node))

	memcpy(line, plm_prefix.s_str, plm_prefix.s_len);
	line[plm_prefix.s_len] = 0;
	strcat(line, "conf/plume.conf");
	
	fp = fopen(line, "r");
	if (!fp) {
		plm_log_syslog("configure file does't exsit: %s %s\n",
					   line, strerror(errno));
		return (-1);
	}

	plm_mempool_init(&pool, PLM_PAGESIZE, malloc, free);
	PLM_STACK_INIT(&ctx_stack);

	do {
		int i, n, ctx_num;
		char *newline;
		plm_conf_parse_state_t stat;

		if (!fgets(line, sizeof(line), fp))
			break;

		newline = strchr(line, '\n');
		if (newline)
			*newline = 0;
		
		n = strlen(line);
		if (n == 0)
			continue;

		if (line[0] == '#')
			continue;

		for (i = 0; i < n; i++) {
			if (!isspace(line[i]))
				break;
		}

		if (line[i] == '#')
			continue;

		while (n > i) {
			if (!isspace(line[n]))
				break;
			--n;
		}

		var_stack.vs_block_num = block;
		stat = plm_conf_parse_line(&curr_ctx_node, &pool, line+i, n-i);
		
		switch (stat) {
		case CPS_BLOCK_BEG:
			if (!curr_ctx_node) {
				plm_log_syslog("block context can't be null");
				abort();
			}

			PLM_STACK_PUSH(&ctx_stack, curr_ctx_node);
			block++;
			plm_conf_var_block_push(&pool);
			break;

		case CPS_BLOCK_END:
			PLM_STACK_POP(&ctx_stack);
			PLM_STACK_LEN(&ctx_num, &ctx_stack);
			if (ctx_num > 0)
				PLM_STACK_TOP(&curr_ctx_node, &ctx_stack);
			else
				curr_ctx_node = NULL;

			block--;
			plm_conf_var_block_pop();
			break;

		case CPS_LINE_END:
			break;

		case CPS_INVALID:
		default:
			plm_log_syslog("parse configure line: (%s) failed\n", line);
			abort();
		}
	} while (!feof(fp));

	if (block > 0) {
		plm_log_syslog("parse configure file failed");
		abort();
	}
	
	plm_mempool_destroy(&pool);
	fclose(fp);
	return (0);
}

static plm_conf_parse_state_t 
plm_conf_parse_state_ins(int ch)
{
	int stat;

	switch (ch) {
	case ' ':
		stat = CPS_SPACE;
		break;

	case '{':
		stat = CPS_BLOCK_BEG;
		break;

	case '}':
		stat = CPS_BLOCK_END;
		break;

	case '$':
		stat = CPS_VAR;
		break;

	default:
		stat = CPS_CHAR;
		break;
	}

	return (stat);
}

static plm_conf_parse_state_t 
plm_conf_parse_state_var(int ch)
{
	int stat;

	switch (ch) {
	case '$':
		stat = CPS_VAR;
		break;

	case '=':
		stat = CPS_ASSIGN;
		break;

	default:
		stat = CPS_CHAR;
		break;
	}

	return (stat);
}

static int plm_conf_var_cmp(void *n, void *data)
{
	int rc = 0;
	struct plm_var_elem *e;
	plm_stack_node_t *node;
	plm_string_t *name;

	node = (plm_stack_node_t *)n;
	name = (plm_string_t *)data;

	e = (struct plm_var_elem *)node;
	if (e->ve_is_spec)
		return (rc);
	
	rc = plm_strcmp(name, &e->ve_u.ve_data.ve_name);
	return (rc == 0);
}

static int
plm_conf_get_var_value(plm_string_t **pp, struct plm_mempool *p,
					   void *ctx, plm_string_t *var)
{
	plm_stack_node_t *node = NULL;

	PLM_STACK_SEARCH(&node, &var_stack.vs_stack, plm_conf_var_cmp, var);

	if (node) {
		struct plm_var_elem *e;

		e = (struct plm_var_elem *)node;
		*pp = &e->ve_u.ve_data.ve_value;
	}

	return (node ? 0 : -1);
}

static int
plm_conf_set_var_value(struct plm_mempool *p, void *ctx, plm_string_t *name,
					   plm_string_t *value)
{
	struct plm_var_elem *e;

	e = (struct plm_var_elem *)
		plm_mempool_alloc(p, sizeof(struct plm_var_elem));
	if (!e)
		return (-1);

	e->ve_is_spec = 0;
	e->ve_u.ve_data.ve_name = *name;
	e->ve_u.ve_data.ve_value = *value;

	PLM_STACK_PUSH(&var_stack.vs_stack, &e->ve_node);
	return (0);
}

static plm_conf_parse_state_t 
plm_conf_parse_variable(struct plm_mempool *p, void *ctx,
						const char *line, int len)
{
	plm_string_t name = plm_string_null;
	plm_string_t value = plm_string_null;
	plm_conf_parse_state_t last;
	plm_conf_parse_state_t stat = CPS_VAR;
	int i, beg = 1, end = 1;
	const char *err_msg = NULL;
	int var_link = 0;

#define GET_VAR_AND_APPEND() \
	do { \
		plm_string_t *v; \
		plm_string_t vn; \
		plm_strassign(&vn, &line[beg], end-beg, p); \
		if (plm_conf_get_var_value(&v, p, ctx, &name)) \
			goto PARSE_FAILED; \
		plm_strappend(&value, v->s_str, v->s_len, p); \
	} while(0);

	for (i = 1; i < len; i++) {
		stat = plm_conf_parse_state_var(line[i]);
	REPARSE:
		switch (stat) {
		case CPS_ASSIGN:
			/* variable must be have a name */
			if (last == CPS_VAR) {
				err_msg = "variable must be have a name";
				goto PARSE_FAILED;
			}
			
			if (end == beg)
				end = i;

			plm_strassign(&name, &line[beg], end-beg, p);
			last = stat;
			break;

		case CPS_CHAR:
			/* new index of begin */
			if (last == CPS_VAR || last == CPS_ASSIGN)
				beg = i;
			last = stat;
			break;

		case CPS_VAR:
			if (last == CPS_ASSIGN) {
				var_link = 1;
			} else if (last == CPS_CHAR) {
				end = i;

				/* link variables */
				if (!var_link)
					goto PARSE_FAILED;

				GET_VAR_AND_APPEND();
			}

			last = stat;
			break;

		case CPS_LINE_END:
			end = i;

			if (var_link) {
				GET_VAR_AND_APPEND();
			} else {
				plm_strassign(&value, &line[beg], end-beg, p);
			}
			break;
		}

		if (i == len - 1) {
			i = len;
			stat = CPS_LINE_END;
			goto REPARSE;
		}
	}

	if (name.s_len > 0) {
		plm_conf_set_var_value(p, ctx, &name, &value);
		return (stat);
	}

PARSE_FAILED:
	plm_log_syslog("invalid variable definition: (%s), error=%s\n",
				   line, err_msg ? err_msg : "null");
	abort();

	/* never reach */
	return (-1);
}

static int plm_conf_cmd_line_push(void **ctx, struct plm_cmd_line *cl,
								  struct plm_mempool *p,
								  const char *token, int len)
{
	struct plm_cmd_param *param;

	if (cl->cl_stat == 0) {
		PLM_DLIST_INIT(&cl->cl_params);

		cl->cl_name.s_str = (char *)plm_mempool_alloc(p, len);
		cl->cl_name.s_len = len;
		if (cl->cl_name.s_str) {
			struct plm_cmd *cmd;

			memcpy(cl->cl_name.s_str, token, len);
			cl->cl_stat = 1;

			/* find the command and create context if need */
			if (plm_plugin_cmd_search(&cmd, &cl->cl_name)) {
				plm_log_syslog("find command failed\n");
				return (-1);
			}

			if (cmd->c_type == CT_BLOCK) {
				void *parent = *ctx;
				*ctx = cmd->c_create_ctx(*ctx);
				plm_ctx_register(cmd->c_plg, parent, *ctx, cmd->c_destroy_ctx);
			}
			
			if (!(*ctx)) {
				plm_log_syslog("context null");
				return (-1);
			}
		}

		return (cl->cl_stat == 0 ? -1 : 0);
	}

	param = (struct plm_cmd_param *)
		plm_mempool_alloc(p, sizeof(struct plm_cmd_param));
	if (param) {
		plm_strassign(&param->cp_data, token, len, p);
		PLM_DLIST_ADD_BACK(&cl->cl_params, &param->cp_node);
	}

	return (param ? 0 : -1);
}

static int plm_conf_set_cmd(struct plm_cmd_line *cl, void *ctx)
{
	int rc = 0;
	struct plm_cmd *cmd;

	if (plm_plugin_cmd_search(&cmd, &cl->cl_name))
		return (-1);

	if (cmd->c_set_cmd)
		rc = cmd->c_set_cmd(ctx, &cl->cl_params);
	
	return (rc);
}

static plm_conf_parse_state_t 
plm_conf_parse_ins(void **ctx, struct plm_mempool *p, char *line, int len)
{
	plm_conf_parse_state_t stat, last;
	struct plm_cmd_line cl;
	const char *err_msg = NULL;
	int beg = 0, end = 0;
	int var = 0;
	int i;

#define GET_VAR_AND_PUSH() \
	do { \
		plm_string_t *v; \
		plm_string_t var_name; \
		var_name.s_str = &line[beg]; \
		var_name.s_len = end-beg; \
		if (plm_conf_get_var_value(&v, p, *ctx, &var_name)) { \
			err_msg = "can find variable"; \
			goto PARSE_FAILED; \
		} \
		if (plm_conf_cmd_line_push(ctx, &cl, p, v->s_str, v->s_len)) {	\
			err_msg = "push command failed"; \
			goto PARSE_FAILED; \
		} \
		var = 0; \
	} while (0)

	cl.cl_stat = 0; 
	last = CPS_NULL;

	for (i = 0; i < len; i++) {
		stat = plm_conf_parse_state_ins(line[i]);
	REPARSE:		
		switch (stat) {
		case CPS_BLOCK_BEG:
			if (last == CPS_CHAR) {
				end = i;

				if (var) {
					GET_VAR_AND_PUSH();
				} else {
					if (plm_conf_cmd_line_push(ctx, &cl, p,
											   &line[beg], end-beg))
						goto PARSE_FAILED;
				}
			} else if (last == CPS_VAR || last == CPS_BLOCK_END) {
				goto PARSE_FAILED;
			}

			last = stat;
			break;

		case CPS_BLOCK_END:
			if (last != CPS_NULL) {
				err_msg = "'}' must be single line";
				goto PARSE_FAILED;
			}

			last = stat;
			break;

		case CPS_CHAR:
			if (last == CPS_BLOCK_BEG) {
				err_msg = "text can't follow '{'";
				goto PARSE_FAILED;
			} else if (last == CPS_BLOCK_END) {
				err_msg = "'}' must be single line";
				goto PARSE_FAILED;
			}

			if (last == CPS_SPACE || last == CPS_VAR)
				beg = i;

			last = stat;
			break;

		case CPS_SPACE:
			if (last != CPS_SPACE
				&& last != CPS_BLOCK_BEG
				&& last != CPS_BLOCK_END) {

				end = i;
				if (last == CPS_VAR) {
					err_msg = "invalid variable name";
					goto PARSE_FAILED;
				} else {
					if (var) {
						GET_VAR_AND_PUSH();
					} else {
						if (plm_conf_cmd_line_push(ctx, &cl, p, &line[beg],
												   end-beg))
							goto PARSE_FAILED;
					}
				}

				last = stat;
			}
			break;

		case CPS_VAR:
			if (last != CPS_SPACE) {
				err_msg = "$ is keyword";
				goto PARSE_FAILED;
			}
			var = 1;
			last = stat;
			break;

		case CPS_LINE_END:
			if (last == CPS_CHAR) {
				end = i;

				if (var) {
					GET_VAR_AND_PUSH();
				} else {
					if (plm_conf_cmd_line_push(ctx, &cl, p,
											   &line[beg], end-beg))
						goto PARSE_FAILED;
				}
			}

			if (last == CPS_VAR) {
				err_msg = "$ is keyword";
				goto PARSE_FAILED;
			}
			break;
		}

		if (i == len - 1 && stat != CPS_BLOCK_END && stat != CPS_BLOCK_BEG) {
			i = len;
			stat = CPS_LINE_END;
			goto REPARSE;
		}
	}

	plm_conf_set_cmd(&cl, *ctx);

	if (stat == CPS_BLOCK_BEG || stat == CPS_BLOCK_END)
		return (stat);
	return (CPS_LINE_END);

PARSE_FAILED:
	plm_log_syslog("configure line failed: (%s), error=%s\n",
				   line, err_msg ? err_msg : "null");
	return (CPS_INVALID);
}

static plm_conf_parse_state_t
plm_conf_parse_line(plm_stack_node_t **n, struct plm_mempool *p,
					char *line, int len)
{
	plm_conf_parse_state_t rc;
	struct plm_block_ctx *curr_ctx;

	if (*n) {
		curr_ctx = (struct plm_block_ctx *)
			(((char *)(*n)) - offsetof(struct plm_block_ctx, bc_node));
	}

	if (line[0] == '$') {
		plm_conf_parse_variable(p, curr_ctx->bc_data, line, len);
		rc = CPS_LINE_END;
	} else {
		void *ctx = NULL;

		if (curr_ctx)
			ctx = curr_ctx->bc_data;
		
		rc = plm_conf_parse_ins(&ctx, p, line, len);

		if (!curr_ctx) {
			curr_ctx = (struct plm_block_ctx *)
				plm_mempool_alloc(p, sizeof(struct plm_block_ctx));
		}

		if (curr_ctx) {
			curr_ctx->bc_data = ctx;
			*n = &curr_ctx->bc_node;
		}
	}

	return (rc);
}

void plm_conf_var_block_push(struct plm_mempool *p)
{
	struct plm_var_elem *e;

	e = (struct plm_var_elem *)
		plm_mempool_alloc(p, sizeof(struct plm_var_elem));
	if (!e)
		abort();

	e->ve_is_spec = 1;
	e->ve_u.ve_block_num = var_stack.vs_block_num;
	if (var_stack.vs_block_num == 0)
		PLM_STACK_INIT(&var_stack.vs_stack);

	PLM_STACK_PUSH(&var_stack.vs_stack, &e->ve_node);
}

void plm_conf_var_block_pop()
{
	for (;;) {
		int len;
		plm_stack_node_t *n;
		struct plm_var_elem *e;

		PLM_STACK_LEN(&len, &var_stack.vs_stack);
		if (len == 0)
			break;
		
		PLM_STACK_TOP(&n, &var_stack.vs_stack);
		PLM_STACK_POP(&var_stack.vs_stack);
		
		e = (struct plm_var_elem *)n;
		if (e->ve_is_spec)
			break;
	}
}
