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

#ifndef _PLM_MEMPOOL_H
#define _PLM_MEMPOOL_H

#include "plm_list.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PLM_PAGESIZE 4096
	
struct plm_mempool {
	struct plm_list m_blk_list;

	struct {
		unsigned char m_zero_memory:1;
	} m_onoff;

	void *(*m_alloc)(size_t);
	void (*m_free)(void *);
	char *m_curr;
	size_t m_curr_free;
	size_t m_init_blksz;
};

/* init memory pool, all the memory will be free when we destroy the pool
 * @pool -- the pool
 * @blk_sz -- init block size
 * @alloc -- memory allocate function
 * @mfree -- memory free function
 * return the pointer to pool
 */
struct plm_mempool *plm_mempool_init(struct plm_mempool *pool, size_t blk_sz,
									 void *(*alloc)(size_t),
									 void (*mfree)(void *));

/* enable feature */
#define plm_mempool_enable(p, z) \
	(p)->m_onoff.m_zero_memory = z;

/* destory memory pool and free all the blocks
 * @pool -- the correct pool
 * return void
 */
void plm_mempool_destroy(struct plm_mempool *pool);

/* allocate
 * @pool -- the correct pool
 * @objsz -- object size
 * return the start of memory pointer or NULL
 */
void *plm_mempool_alloc(struct plm_mempool *pool, size_t objsz);

#define plm_mempool_alloc_type(t, p) ((t) *)plm_mempool_alloc(p, sizeof((t)))

#ifdef __cplusplus
}
#endif
	
#endif

