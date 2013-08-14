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

#ifndef _PLM_HASH_H
#define _PLM_HASH_H

#include <stdint.h>

#include "plm_list.h"

#ifdef __cplusplus
extern "C" {
#endif

struct plm_hash_node {
	plm_list_node_t hn_node;
	void *hn_key;
	void *hn_value;
};

struct plm_hash_bucket {
	plm_list_t hb_list;
};

struct plm_hash {
	void *(*h_alloc)(size_t, void *);
	void (*h_free)(void *, void *);
	uint32_t (*h_key)(void *, uint32_t);
	int (*h_cmp)(void *, void *);
	void *h_data;
	uint32_t h_len;
	uint32_t h_bucket_num;
	struct plm_hash_bucket *h_bucket;
};

/* init hash returns 0 on success, else -1 */
int plm_hash_init(struct plm_hash *hash, uint32_t max_bucket,
				  uint32_t (*key)(void *, uint32_t), int (*cmp)(void *, void *),
				  void *(*alloc)(size_t, void *), void (*free)(void *, void *),
				  void *data);

/* insert hash node */
void plm_hash_insert(struct plm_hash *hash, struct plm_hash_node *node);

/* find hash node by key returns 0 on success, else -1 */
int plm_hash_find(struct plm_hash_node **pp, struct plm_hash *hash,
				  void *key, void *value);

/* delete all values with the same key */
void plm_hash_delete(struct plm_hash *hash, void *key);

/* remove the node with specifiy key and value */	
void plm_hash_delete2(struct plm_hash *hash, void *key, void *value);

void plm_hash_foreach(struct plm_hash *hash, void *data,
					  void (*fn)(void *key, void *value, void *data));	

/* destroy hash and free the bucket */
#define plm_hash_destroy(hash)					\
	do {										\
		(hash)->h_free((hash)->h_bucket, (hash)->h_data);	\
		(hash)->h_len = 0;						\
	} while (0)

#ifdef __cplusplus
}
#endif

#endif
