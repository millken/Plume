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

#include "plm_http_errlog.h"
#include "plm_http_backend.h"

static int curr;
static int num;
struct sockaddr_in *backend_addr;

int plm_http_backend_init(struct plm_http_ctx *c)
{
	int n, i;

	n = PLM_LIST_LEN(&c->hc_backends);
	if (n == 0) {
		plm_log_syslog("can't find any backend server");
		return (-1);
	}
	
	curr = 0;
	backend_addr = (struct sockaddr_in *)malloc(n * sizeof(*backend_addr));
	if (backend_addr) {
		struct plm_http_backend *bk;

		bk = (struct plm_http_backend *)PLM_LIST_FRONT(&c->hc_backends);
		for (i = 0; i < n; i++) {
			memcpy(backend_addr + i, &bk->hb_addr, sizeof(*backend_addr));
			bk = (struct plm_http_backend *)PLM_LIST_NEXT(&bk->hb_node);
		}

		num = n;
	}

	return (backend_addr ? 0 : -1);
}

int plm_http_backend_destroy()
{
	curr = 0;
	free(backend_addr);
	backend_addr = NULL;
}

int plm_http_backend_select(struct plm_http_req *r)
{
	int n, m;

	n = plm_atomic_int_get(&curr);
	m = n % num;
	plm_atomic_int_inc(&curr);

	r->hr_backend = &backend_addr[m];
	return (0);
}

int plm_http_backend_forward(struct plm_http_req *r)
{
}

