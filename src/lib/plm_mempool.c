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

#include <string.h>

#include "plm_mempool.h"

struct plm_memnode {
	struct plm_list_node m_node;
	char m_buf[0];
};

#define PLM_STRUCT_OFFSET(s, m)	(size_t)&(((s *)0)->m)

/* init memory pool, all the memory will be free when we destroy the pool
 * @pool -- the pool
 * @blk_sz -- init block size
 * @alloc -- memory allocate function
 * @mfree -- memory free function
 * return the pointer to pool
 */
struct plm_mempool *plm_mempool_init(struct plm_mempool *pool, size_t blk_sz,
									 void *(*alloc)(size_t),
									 void (*mfree)(void *))
{
	PLM_LIST_INIT(&pool->m_blk_list);
	memset(&pool->m_onoff, 0, sizeof(pool->m_onoff));
	pool->m_curr = NULL;
	pool->m_curr_free = 0;
	pool->m_init_blksz = blk_sz;
	pool->m_alloc = alloc;
	pool->m_free = mfree;
	return (pool);
}

/* destory memory pool and free all the blocks
 * @pool -- the correct pool
 * return void
 */
void plm_mempool_destroy(struct plm_mempool *pool)
{
	void *blk;
	struct plm_list_node *node;

	while (PLM_LIST_LEN(&pool->m_blk_list) > 0) {
		node = PLM_LIST_FRONT(&pool->m_blk_list);
		PLM_LIST_DEL_FRONT(&pool->m_blk_list);
		blk = (char *)node - PLM_STRUCT_OFFSET(struct plm_memnode, m_node);
		pool->m_free(blk);
	}

	memset(pool, 0, sizeof(struct plm_mempool));
}

/* allocate
 * @pool -- the correct pool
 * @objsz -- object size
 * return the start of memory pointer or NULL
 */
void *plm_mempool_alloc(struct plm_mempool *pool, size_t objsz)
{
	char *data;
	
	if (!pool->m_curr) {
		struct plm_memnode *node;
		size_t blksz = pool->m_init_blksz + sizeof(struct plm_memnode);
		size_t m = blksz % sizeof(void *);
		if (m != 0)
			blksz += sizeof(void *) - m;

		node = (struct plm_memnode *)pool->m_alloc(blksz);
		if (!node)
			return (NULL);

		pool->m_curr = node->m_buf;
		pool->m_curr_free = blksz - sizeof(struct plm_memnode);
		PLM_LIST_ADD_FRONT(&pool->m_blk_list, &node->m_node);
	}

	if (objsz > pool->m_curr_free) {
		size_t m;
		size_t blksz = pool->m_init_blksz;
		struct plm_memnode *node;

		while (blksz < objsz)
			blksz += pool->m_init_blksz;

		blksz += sizeof(struct plm_memnode);
		m = blksz % sizeof(void *);
		if (m != 0)
			blksz += sizeof(void *) - m;

		node = (struct plm_memnode *)pool->m_alloc(blksz);
		if (!node)
			return (NULL);

		pool->m_curr = node->m_buf;
		pool->m_curr_free = blksz - sizeof(struct plm_memnode);
		PLM_LIST_ADD_FRONT(&pool->m_blk_list, &node->m_node);
	}

	data = pool->m_curr;
	pool->m_curr += objsz;
	pool->m_curr_free -= objsz;

	return (data);
}

