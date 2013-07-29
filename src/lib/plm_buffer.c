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
#include "plm_lookaside_list.h"
#include "plm_buffer.h"

static struct plm_lookaside_list mem_list[MEM_END];
static size_t mem_size[MEM_END] = {
	SIZE_8K,
	SIZE_4K,
	SIZE_2K,
	SIZE_1K
};

/* init buffers */
void plm_buffer_init(int thrdsafe, int tagchk, int zeromem)
{
	int i;
	
	for (i = 0; i < MEM_END; i++) {
		plm_lookaside_list_init(&mem_list[i], -1, mem_size[i],
								-1, malloc, free);
		plm_lookaside_list_enable(&mem_list[i], zeromem, tagchk, thrdsafe);
	}
}

/* alloc memory buffer 8k, 4k, 2k, 1k
 * @buf -- buffer to hold memory
 * @type -- buffer type
 * return address of buf on success or NULL on error
 */
struct plm_buffer *plm_buffer_alloc(struct plm_buffer *buf, int type)
{
	size_t size = 0;

	switch (type) {
	case MEM_8K:
		size = SIZE_8K;
		break;

	case MEM_4K:
		size = SIZE_4K;
		break;

	case MEM_2K:
		size = SIZE_2K;
		break;

	case MEM_1K:
		size = SIZE_1K;
		break;
	}
	
	buf->b_data = plm_lookaside_list_alloc(&mem_list[type], NULL);
	buf->b_size = buf->b_data ? size : 0;
	return (buf->b_data ? buf : NULL);
}

/* free memory */
void plm_buffer_free(int type, struct plm_buffer *buf)
{
	if (!buf->b_data)
		return;
	
	plm_lookaside_list_free(&mem_list[type], buf->b_data, NULL);
	buf->b_data = NULL;
	buf->b_size = 0;
}

/* destroy pool and free all memory */
void plm_buffer_destroy()
{
	int i;

	for (i = 0; i < MEM_END; i++)
		plm_lookaside_list_destroy(&mem_list[i]);
}
