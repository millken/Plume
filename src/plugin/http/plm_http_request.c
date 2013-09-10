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
#include <errno.h>

#include "plm_comm.h"
#include "plm_hash.h"
#include "plm_event.h"
#include "plm_log.h"
#include "plm_string.h"
#include "plm_http_errlog.h"
#include "plm_http_event_io.h"
#include "plm_buffer.h"
#include "plm_http.h"
#include "plm_http_plugin.h"
#include "plm_http_backend.h"
#include "plm_http_request.h"

static int http_server;
static void plm_http_read_req(void *, int);

static void
plm_http_body_process(void *req, plm_string_t *cnt)
{
}

static uint32_t
plm_http_field_key(void *data, uint32_t max)
{
	uint32_t key;
	plm_string_t *v;

	v = (plm_string_t *)data;
	return (v->s_str[0] % max);
}

static int
plm_http_field_cmp(void *data1, void *data2)
{
	plm_string_t *v1, *v2;

	v1 = (plm_string_t *)data1;
	v2 = (plm_string_t *)data2;
	return plm_strcasecmp(v1, v2);
}

static void *
plm_http_palloc(size_t n, void *data)
{
	struct plm_mempool *p;

	p = (struct plm_mempool *)data;
	return plm_mempool_alloc(p, n);
}

static void
plm_http_pfree(void *p, void *data) {}

static int
plm_http_on_reqline(enum plm_http_mthd mthd, const plm_string_t *url,
					enum plm_http_ver ver, void *data)
{
	struct plm_http_req *r;
	struct plm_http_conn *c;

	c = (struct plm_http_conn *)data;
	r = (struct plm_http_req *)plm_mempool_alloc(&c->hc_pool, sizeof(*r));
	if (r) {
		memset(r, 0, sizeof(*r));
		plm_strzassign(&r->hr_url, url->s_str, url->s_len, &c->hc_pool);
		if (r->hr_url.s_str) {
			struct plm_http_url u;
			
			r->hr_conn = c;
			r->hr_mthd = mthd;
			r->hr_ver = ver;

			plm_http_parser_url(&u, &r->hr_url);

			if (u.hu_host.s_len > 0) {
				plm_strzassign(&r->hr_host, u.hu_host.s_str,
							   u.hu_host.s_len, &c->hc_pool);
			}
			
			if (u.hu_port.s_len > 0)
				r->hr_port = plm_str2s(&u.hu_port);

			plm_hash_init(&r->hr_fields, 26, plm_http_field_key,
						  plm_http_field_cmp, plm_http_palloc, plm_http_pfree,
						  &c->hc_pool);

			PLM_LIST_ADD_FRONT(&c->hc_reqs, &r->hr_node);

			/* set the parser user data to request */
			c->hc_parser.hp_data = r;
			
			return (0);
		}
	}

	return (-1);
}

static int
plm_http_on_field(const plm_string_t *k, const plm_string_t *v, void *data)
{
	static plm_string_t cs = plm_string("Connection");
	static plm_string_t kps = plm_string("keep-alive");
	static plm_string_t pcs = plm_string("Proxy-Connection");
	static plm_string_t cl = plm_string("Content-Length");

	struct plm_http_req *r;
	struct plm_mempool *p;
	plm_string_t *nk, *nv;
	struct plm_hash_node *node;

	r = (struct plm_http_req *)data;
	p = &r->hr_conn->hc_pool;

	switch (k->s_str[0]) {
	case 'C':
	case 'c':
		if (!plm_strcasecmp(k, &cs) && !plm_strcasecmp(v, &kps))
			r->hr_flags.hr_hdr_kpalv_on = 1;
		else if (!plm_strcasecmp(k, &cl))
			r->hr_cntlen = plm_str2ll(v);
		break;

	case 'P':
	case 'p':
		if (!plm_strcasecmp(k, &pcs)) {
			if (plm_strcasecmp(v, &kps))
				r->hr_flags.hr_hdr_kpalv_on = 1;
			k = &cs;
		}
		break;

	case 'H':
	case 'h':
		if (r->hr_host.s_len == 0) {
			char *c = memchr(v->s_str, ':', v->s_len);
			if (c) {
				plm_string_t port, host;
				
				port.s_str = ++c;
				port.s_len = v->s_len - (c - v->s_str);
				r->hr_port = plm_str2s(&port);

				host.s_str = v->s_str;
				host.s_len = v->s_len - port.s_len - 1;

				plm_strzassign(&r->hr_host, host.s_str, host.s_len, p);
			} else {
				plm_strzassign(&r->hr_host, v->s_str, v->s_len, p);
			}
		}
		break;
	}

	nk = nv = NULL;
	plm_strzalloc(&nk, k->s_str, k->s_len, p);
	plm_strzalloc(&nv, v->s_str, v->s_len, p);

	if (!nk || !nv) {
		PLM_FATAL("strzalloc failed");
		return (-1);
	}

	node = (struct plm_hash_node *)plm_mempool_alloc(p, sizeof(*node));

	if (!node) {
		PLM_FATAL("mempool alloc failed");
		return (-1);
	}
	
	node->hn_key = nk;
	node->hn_value = nv;

	plm_hash_insert(&r->hr_fields, node);
	return (0);
}

static void
plm_http_on_hdr_done(void *data)
{
	struct plm_http_req *r;
	struct plm_http_conn *c;

	if (r->hr_port == 0)
		r->hr_port = 80;

	c = r->hr_conn;
	plm_http_parser_init(&c->hc_parser, c);

	if (r->hr_cntlen > 0) {
		c->hc_body.hb_data = r;
		c->hc_body.hb_callback = plm_http_body_process;
	}
}

static void plm_http_conn_free(void *data)
{
	struct plm_http_conn *conn;

	conn = (struct plm_http_conn *)data;
	if (conn->hc_in.hc_data) {
		int pt;
		
		plm_mempool_destroy(&conn->hc_pool);		

		switch (conn->hc_in.hc_size) {
		case SIZE_1K:
			pt = MEM_1K;
			break;
		case SIZE_2K:
			pt = MEM_2K;
			break;
		case SIZE_4K:
			pt = MEM_4K;
			break;
		case SIZE_8K:
			pt = MEM_8K;
			break;
		default:
			PLM_FATAL("unknown buffer type");
			break;
		}
		
		plm_buffer_free(pt, conn->hc_in.hc_data);
	}
}

static struct plm_http_conn *
plm_http_conn_alloc(struct plm_http_ctx *ctx)
{
	struct plm_http_conn *conn;

	conn = (struct plm_http_conn *)
		plm_lookaside_list_alloc(&ctx->hc_conn_pool, NULL);
	if (conn) {
		memset(conn, 0, sizeof(*conn));
		conn->hc_in.hc_data = plm_buffer_alloc(MEM_1K);
		if (conn->hc_in.hc_data) {
			conn->hc_ctx = ctx;
			conn->hc_in.hc_size = SIZE_1K;

			plm_mempool_init(&conn->hc_pool, 512, malloc, free);

			conn->hc_cch.cch_handler = plm_http_conn_free;
			conn->hc_cch.cch_data = conn;

			/* init hooks */
			conn->hc_parser.hp_on_req_line = plm_http_on_reqline;
			conn->hc_parser.hp_on_status_line = NULL;
			conn->hc_parser.hp_on_field = plm_http_on_field;
			conn->hc_parser.hp_on_hdr_done = plm_http_on_hdr_done;

			/* user data with conn
			 * change the user data in plm_http_on_reqline or status_line
			 * and change back to conn in plm_http_on_hdr_done
			 */
			plm_http_parser_init(&conn->hc_parser, conn);
		} else {
			plm_lookaside_list_free(&ctx->hc_conn_pool, conn, NULL);
			conn = NULL;
		}
	}
	
	return (conn);
}

void plm_http_accept(void *data, int fd)
{
	int retry = 0;
	int clifd, err;
	struct sockaddr_in addr;
	struct plm_http_ctx *ctx;
	struct plm_http_conn *conn;

	do {
		ctx = (struct plm_http_ctx *)data;

		clifd = plm_comm_accept(fd, &addr, 1);
		if (clifd < 0) {
			if (!plm_comm_ignore(errno))
				PLM_FATAL("plm_comm_accept failed");
			break;
		}

		retry++;
		conn = plm_http_conn_alloc(ctx);
		if (!conn) {
			plm_comm_close(clifd);
			PLM_FATAL("plm_http_conn_alloc failed");
			break;
		}

		conn->hc_fd = clifd;
		memcpy(&conn->hc_addr, &addr, sizeof(conn->hc_addr));
		plm_comm_add_close_handler(clifd, &conn->hc_cch);

		PLM_EVT_DRV_READ(clifd, conn, plm_http_read_req);
	} while (1);

	err = plm_event_io_read2(fd, data, plm_http_accept);
	if (err) {
		PLM_FATAL("plm_event_io_read on listen fd failed: %s",
				  strerror(errno));
		exit(-1);
	}   
}

static void
plm_http_schedule_reply_done(void *data, char *buf, size_t n, int state)
{
}

static void
plm_http_schedule_reply(struct plm_http_conn *c, int err)
{
	static plm_string_t badreq = plm_string("");
	static plm_string_t errfwd = plm_string("");
	static plm_string_t nobackend = plm_string("");

	if (err == PLM_ERR_BACKEND_SELECT)
		c->hc_flags.hc_nobackend = 1;
	else if (err == PLM_ERR_BACKEND_FWD)
		c->hc_flags.hc_errfwd = 1;
	else if (err == PLM_ERR_BADREQ)
		c->hc_flags.hc_badreq = 1;
	
	if (PLM_LIST_LEN(&c->hc_resps) > 0)
		return;

	c->hc_wrevt.hw_fn = plm_http_schedule_reply_done;
	c->hc_wrevt.hw_data = c;
	c->hc_wrevt.hw_off = 0;
	
	if (c->hc_flags.hc_badreq) {
		c->hc_wrevt.hw_buf = badreq.s_str;
		c->hc_wrevt.hw_len = badreq.s_len;
	} else if (c->hc_flags.hc_errfwd) {
		c->hc_wrevt.hw_buf = errfwd.s_str;
		c->hc_wrevt.hw_len = errfwd.s_len;		
	} else if (c->hc_flags.hc_nobackend) {
		c->hc_wrevt.hw_buf = nobackend.s_str;
		c->hc_wrevt.hw_len = nobackend.s_len;				
	}
		
	return plm_http_event_write(c->hc_fd, &c->hc_wrevt);
}

static void
plm_http_req_process(struct plm_http_req *r)
{
	struct plm_http_conn *c;
	int et = PLM_ERR_BACKEND_SELECT;

	c = r->hr_conn;
	if (plm_http_backend_select(r)) {
		if (plm_http_backend_forward(r))
			return;

		et = PLM_ERR_BACKEND_FWD;
	}

	shutdown(c->hc_fd, SHUT_RD);
	plm_http_schedule_reply(c, et);
}

void plm_http_read_req(void *data, int fd)
{
	int rc, n;
	char *buf;
	size_t size, off, parsed;
	struct plm_http_conn *conn;
	struct plm_http_req *req;
	plm_string_t s;

	conn = (struct plm_http_conn *)data;
	off = conn->hc_in.hc_offset;
	size = conn->hc_in.hc_size;
	buf = conn->hc_in.hc_data;

	n = plm_comm_read(fd, buf + off, size - off);
	if (n < 0) {
		if (plm_comm_ignore(errno)) {
			plm_event_io_read(fd, data, plm_http_read_req);
		} else {
			PLM_FATAL("plm_comm_read failed: %s", strerror(errno));
			plm_comm_close(fd);
		}
		return;
	}

	if (n == 0) {
		PLM_TRACE("connection closed");
		if (PLM_LIST_LEN(&conn->hc_reqs) == 0)
			plm_comm_close(fd);
		else
			conn->hc_flags.hc_eof = 1;
		return;
	}

	s.s_str = buf;
	s.s_len = n;

	if (conn->hc_body.hb_callback) {
		conn->hc_body.hb_callback(conn->hc_body.hb_data, &s);
		return;
	}
	
	rc = plm_http_parser_req(&conn->hc_parser, &s);
	if (rc == PLM_HTTP_PARSE_ERROR) {
		PLM_TRACE("bad request");
		shutdown(fd, SHUT_RD);
		plm_http_schedule_reply(conn, PLM_ERR_BADREQ);
		return;
	}

	parsed = conn->hc_parser.hp_parsed;
	conn->hc_in.hc_offset = n - parsed;
	if (conn->hc_in.hc_offset > 0)
		memmove(buf, buf + parsed, conn->hc_in.hc_offset);

	if (rc == PLM_HTTP_PARSE_DONE) {
		req = (struct plm_http_req *)PLM_LIST_FRONT(&conn->hc_reqs);
		plm_http_req_process(req);
	}

	PLM_EVT_DRV_READ(fd, data, plm_http_read_req);
}

int plm_http_open_server(struct plm_http_ctx *ctx)
{
	int err = -1;
	int port = ctx->hc_port;
	int backlog = ctx->hc_backlog;
	const char *ip = ctx->hc_addr.s_str;

	if (plm_http_backend_init(ctx)) {
		plm_log_syslog("backend init failed");
		return (err);
	}
	
	http_server = plm_comm_open(PLM_COMM_TCP, NULL, 0, 0, port, ip,
								backlog, 1, 1);
	if (http_server < 0) {
		plm_log_syslog("can't open http plugin listen fd: %s:%d", ip, port);
	} else {
		err = plm_event_io_read2(http_server, ctx, plm_http_accept);
		if (err)
			plm_log_syslog("plm_event_io_read failed on listen fd");
	}

	return (err);
}

int plm_http_close_server()
{
	int err = 0;
	
	if (http_server > 0) {
		err = plm_comm_close(http_server);
		http_server = -1;
	}

	plm_http_backend_destroy();
	return (err);
}

