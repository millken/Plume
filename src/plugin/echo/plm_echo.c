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
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>

#include "plm_comm.h"
#include "plm_log.h"
#include "plm_lookaside_list.h"
#include "plm_mempool.h"
#include "plm_plugin.h"

static void *plm_echo_ctx_create(void *parent);
static void plm_echo_ctx_destroy(void *ctx);
static int plm_echo_str_set(void *ctx, plm_dlist_t *params);
static int plm_echo_port_set(void *ctx, plm_dlist_t *params);

struct plm_plugin echo_plugin;
static struct plm_cmd echo_cmds[] = {
	{
		&echo_plugin,
		plm_string("echo"),
		CT_BLOCK,
		NULL,
		plm_echo_ctx_create,
		plm_echo_ctx_destroy
	},
	{
		&echo_plugin,
		plm_string("echo_str"),
		CT_INSTRUCTION,
		plm_echo_str_set,
		NULL,
		NULL
	},
	{
		&echo_plugin,
		plm_string("echo_port"),
		CT_INSTRUCTION,
		plm_echo_port_set,
		NULL,
		NULL
	},
	{0}
};

static int plm_echo_on_work_proc_start(struct plm_ctx_list *ctx);
static void plm_echo_on_work_proc_exit(struct plm_ctx_list *ctx);

struct plm_plugin echo_plugin = {
	plm_echo_on_work_proc_start,
	plm_echo_on_work_proc_exit,
	NULL,
	NULL,
	echo_cmds
};

static int echo_server_fd;

struct plm_echo_conf {
	plm_string_t ec_echostr;
	short ec_port;
};

void *plm_echo_ctx_create(void *parent)
{
	struct plm_echo_conf *conf;
	
	conf = malloc(sizeof(struct plm_echo_conf));
	if (conf)
		memset(conf, 0, sizeof(struct plm_echo_conf));
	return (conf);
}

void plm_echo_ctx_destroy(void *ctx)
{
	struct plm_echo_conf *conf;

	conf = (struct plm_echo_conf *)ctx;
	if (conf->ec_echostr.s_str)
		free(conf->ec_echostr.s_str);
	free(ctx);
}

int plm_echo_str_set(void *ctx, plm_dlist_t *params)
{
	int len;
	char *str;
	struct plm_echo_conf *conf;
	struct plm_cmd_param *param;

	conf = (struct plm_echo_conf *)ctx;
	param = (struct plm_cmd_param *)PLM_DLIST_FRONT(params);

	len = param->cp_data.s_len;
	str = (char *)malloc(len);
	if (str) {
		memcpy(str, param->cp_data.s_str, len);
		conf->ec_echostr.s_len = len;
		conf->ec_echostr.s_str = str;
	}

	return (0);
}

int plm_echo_port_set(void *ctx, plm_dlist_t *params)
{
	struct plm_echo_conf *conf;
	struct plm_cmd_param *param;

	conf = (struct plm_echo_conf *)ctx;
	param = (struct plm_cmd_param *)PLM_DLIST_FRONT(params);

	conf->ec_port = plm_str2s(&param->cp_data);
	return (0);
}

struct plm_echo_ctx {
	struct plm_echo_conf *ec_conf;
};

struct plm_echo_client {
	struct plm_echo_ctx *ec_ctx;
	struct plm_mempool ec_pool;
	char *ec_buf;
	int ec_bufn;
	int ec_bytes_read;
};

static struct plm_echo_ctx ctx;
static struct plm_lookaside_list blk_list;

static void *plm_echo_alloc_client()
{
	struct plm_echo_client *cli;

	cli = (struct plm_echo_client *)plm_lookaside_list_alloc(&blk_list, NULL);
	if (cli) {
		memset(cli, 0, sizeof(*cli));
		plm_mempool_init(&cli->ec_pool, 1024, malloc, free);
	}

	return (cli);
}

static void plm_echo_read(void *data, int fd);
static void plm_echo_write(void *data, int fd);
static void plm_echo_free_client(void *data)
{
	struct plm_echo_client *cli;

	cli = (struct plm_echo_client *)data;
	if (cli) {
		plm_mempool_destroy(&cli->ec_pool);
		plm_lookaside_list_free(&blk_list, cli, NULL);
	}
}

void plm_echo_write(void *data, int fd)
{
	int len, nsend, rc;
	const char *buf;
	struct plm_echo_client *cli;
	struct plm_echo_ctx *ctx;
	struct plm_echo_conf *conf;

	cli = (struct plm_echo_client *)data;
	ctx = cli->ec_ctx;
	conf = ctx->ec_conf;

	if (conf->ec_echostr.s_len > 0) {
		len = conf->ec_echostr.s_len;
		buf = conf->ec_echostr.s_str;
	} else {
		len = cli->ec_bytes_read;
		buf = cli->ec_buf;
	}

	nsend = 0;

	while (nsend < len) {
		rc = plm_comm_write(fd, &buf[nsend], len-nsend);
		if (rc <= 0)
			break;
		nsend += rc;
	}

	if (nsend != len) {
		shutdown(fd, SHUT_WR);
		plm_log_write(PLM_LOG_WARNING,
					  "plm_echo_write: comm write failed=%d", rc);
	} else {
		plm_log_write(PLM_LOG_TRACE,
					  "plm_echo_write: send reply bytes=%d", len);
		if (cli->ec_bufn > 0) {
			plm_echo_read(data, fd);
			return;
		}
	}

	if (plm_event_io_read(fd, data, plm_echo_read)) {
		plm_comm_close(fd);
		plm_log_write(PLM_LOG_FATAL,
					  "plm_echo_write: plm_event_io_read failed");
	}
}

static void plm_echo_reply(void *data, int fd)
{
	if (plm_event_io_write(fd, data, plm_echo_write)) {
		plm_comm_close(fd);
		plm_log_write(PLM_LOG_WARNING,
					  "plm_echo_reply: plm_event_io_write failed");
	}
}

static int plm_echo_eat(int fd, struct plm_echo_client *cli)
{
	char buf[512];
	int count = 0;

	for (;;) {
		int n = plm_comm_read(fd, buf, sizeof(buf));
		if (n <= 0) {
			if (count == 0)
				count = n;
			break;
		}
		count += n;
	}

	return (count);
}

static int plm_echo_read_content(int fd, void *data)
{
	int bufn = 512, n;
	struct plm_echo_client *cli;

	cli = (struct plm_echo_client *)data;
	if (!cli->ec_buf) {
		cli->ec_buf = plm_mempool_alloc(&cli->ec_pool, bufn);
		if (!cli->ec_buf) {
			plm_log_write(PLM_LOG_FATAL, "mempool alloc failed");
			return (-1);
		}

		cli->ec_bufn = bufn;
	}
	
	n = plm_comm_read(fd, cli->ec_buf, cli->ec_bufn);
	if (n > 0)
		cli->ec_bytes_read = n;
	
	return (n);
}

void plm_echo_read(void *data, int fd)
{
	int n;
	struct plm_echo_client *cli = (struct plm_echo_client *)data;

	if (cli->ec_ctx->ec_conf->ec_echostr.s_len > 0)
		n = plm_echo_eat(fd, cli);
	else
		n = plm_echo_read_content(fd, cli);

	if (n > 0) {
		plm_log_write(PLM_LOG_DEBUG, "plm_echo_read: read bytes=%d", n);
		plm_echo_reply(data, fd);
	} else {
		if (n == 0) {
			if (plm_comm_close(fd)) {
				plm_log_write(PLM_LOG_FATAL,
							  "plm_echo_read: plm_comm_close failed: %s",
							  strerror(errno));
			}
			plm_echo_free_client(data);
			plm_log_write(PLM_LOG_TRACE,
						  "plm_echo_read: connection reset=%d", fd);
		} else if (errno == EWOULDBLOCK || errno == EAGAIN) {
			plm_event_io_read(fd, data, plm_echo_read);
			plm_log_write(PLM_LOG_TRACE, "plm_echo_read: no data to read");
		} else {
			plm_comm_close(fd);
			plm_echo_free_client(data);
			plm_log_write(PLM_LOG_FATAL, "plm_echo_read: read failed=%d", fd);
		}
	}
}

static void plm_echo_accept(void *data, int fd)
{
	int clifd;
	struct sockaddr_in addr;

	clifd = plm_comm_accept(fd, &addr, 1);
	
	plm_event_io_read(fd, data, plm_echo_accept);

	if (clifd > 0) {
		struct plm_echo_client *cli;

		cli = plm_echo_alloc_client();
		if (cli) {
			cli->ec_ctx = (struct plm_echo_ctx *)data;
			if (!plm_event_io_read(clifd, cli, plm_echo_read)) {
				plm_log_write(PLM_LOG_TRACE,
							  "plm_echo_accept: accept new connection=%d",
							  clifd);
				return;
			}

			plm_echo_free_client(cli);
		}
	}

	plm_log_write(PLM_LOG_WARNING, "plm_echo_accept: accept failed");
}

int plm_echo_on_work_proc_start(struct plm_ctx_list *cl)
{
	int rc, thrdsafe = 1;
	struct plm_echo_conf *conf;

	conf = (struct plm_echo_conf *)PLM_CTX_LIST_GET_POINTER(cl);

	plm_lookaside_list_init(&blk_list, 128, sizeof(struct plm_echo_client),
							0xEFEFABCD, malloc, free);
	echo_server_fd = plm_comm_open(PLM_COMM_TCP, NULL, 0, 0, conf->ec_port,
								   NULL, 100, 1);
	if (echo_server_fd < 0) {
		plm_log_syslog("can't open echo plugin listen fd");
		return (-1);
	}
	
	ctx.ec_conf = conf;
	rc = plm_event_io_read(echo_server_fd, &ctx, plm_echo_accept);
	if (rc < 0) {
		plm_log_syslog("plm_echo_on_work_proc_start"
					  ": plm_event_io_read failed=%d", rc);
		return (-1);
	}

	return (0);
}

void plm_echo_on_work_proc_exit(struct plm_ctx_list *cl)
{
	plm_comm_close(echo_server_fd);
	plm_lookaside_list_destroy(&blk_list);
}


