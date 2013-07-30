/* Copyright (c) 2011-2013 Xingxing Ke <yykxx@hotmail.com>
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

#include "plm_comm.h"
#include "plm_event.h"
#include "plm_http_request.h"

static void plm_http_read_header(void *, int);
static void plm_http_read_body(void *, int);
static void plm_http_write_header(void *, int);
static void plm_http_write_body(void *, int);
static void plm_http_request_chain_process(struct plm_http_request *);

static struct plm_http_conn *plm_http_conn_alloc(struct plm_http_ctx *,
												 struct sockaddr_in *,
												 int);
static void plm_http_conn_free(void *);

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
			int level = retry > 0 ? PLM_LOG_DEBUG : PLM_LOG_FATAL;
			plm_log_write(level, "plm_comm_accept failed: %s", strerror(errno));
			break;
		}

		retry++;
		conn = plm_http_conn_alloc(ctx, clifd);
		if (!conn) {
			plm_comm_close(clifd);
			plm_log_write(PLM_LOG_FATAL, "plm_http_conn_alloc failed");
			break;
		}

		plm_comm_add_close_handler(clifd, &conn->hc_close_handler);
		err = plm_event_io_read(clifd, conn, plm_http_read_header);
		if (err) {
			plm_comm_close(clifd);
			plm_log_write(PLM_LOG_FATAL, "plm_event_io_read on new fd "
						  "failed: %s", strerror(errno));
			break;
		}
	} while (1);

	err = plm_event_io_read(fd, data, plm_http_accept);
	if (err) {
		plm_log_write(PLM_LOG_FALTA, "plm_event_io_read on listen fd "
					  "failed: %s", strerror(errno));
	}   
}

struct plm_http_conn *plm_http_conn_alloc(struct plm_http_ctx *ctx,
										  struct sockaddr_in *addr,
										  int fd)
{
	struct plm_http_conn *conn;

	conn = (struct plm_http_conn *)plm_lookaside_list_alloc(&ctx->hc_conn_pool,
															NULL);
	if (conn) {
		conn->hc_buf = plm_buffer_alloc(MEM_4K);
		if (conn->hc_buf) {
			conn->hc_size = SIZE_4K;
			conn->hc_offset = 0;
			
			conn->hc_fd = fd;
			memcpy(&conn->hc_addr, addr, sizeof(conn->hc_addr));

			plm_mempool_init(&conn->hc_pool, 512, malloc, free);
			plm_http_proto_init(&conn->hc_request.hr_http_header);
		
			conn->hc_close_handler.cch_handler = plm_http_conn_free;
			conn->hc_close_handler.cch_data = conn;
		} else {
			conn->hc_size = conn->hc_offset = 0;
		}		
	}
	
	return (conn);
}

void plm_http_conn_free(void *data)
{
	struct plm_http_conn *conn;

	conn = (struct plm_http_conn *)data;
	if (conn->hc_buf) {
		plm_buffer_free(conn->hc_buf);
		plm_mempool_destroy(&conn->hc_pool);
	}
}
