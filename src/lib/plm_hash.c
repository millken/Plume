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

#include "plm_hash.h"

/* init hash returns 0 on success, else -1 */
int plm_hash_init(struct plm_hash *hash, uint32_t max_bucket,
				  uint32_t (*key)(void *, uint32_t), int (*cmp)(void *, void *),
				  void *(*alloc)(size_t, void *), void (*free)(void *, void *),
				  void *data)
{
	uint32_t i;
	size_t sz = max_bucket * sizeof(struct plm_hash_bucket);

	hash->h_len = 0;
	hash->h_alloc = alloc;
	hash->h_free = free;
	hash->h_data = data;
	hash->h_key = key;
	hash->h_cmp = cmp;
	hash->h_bucket_num = max_bucket;
	hash->h_bucket = (struct plm_hash_bucket *)alloc(sz, data);

	if (hash->h_bucket) {
		for (i = 0; i < max_bucket; i++)
			PLM_LIST_INIT(&hash->h_bucket[i].hb_list);
	}
	
	return (hash->h_bucket ? 0 : -1);
}

/* insert hash node */
void plm_hash_insert(struct plm_hash *hash, struct plm_hash_node *node)
{
	uint32_t key = hash->h_key(node->hn_key, hash->h_bucket_num);
	PLM_LIST_ADD_FRONT(&hash->h_bucket[key].hb_list, &node->hn_node);
	hash->h_len++;
}

#define value_cmp(x, y) \
	(n = hash->h_cmp(((struct plm_hash_node *)(x))->hn_value, y), n == 0)


/* find hash node by key returns 0 on success, else -1 */
int plm_hash_find(struct plm_hash_node **pp, struct plm_hash *hash,
				  void *key, void *value)
{
	int n;
	uint32_t k;
	plm_list_node_t *ln = NULL;
	struct plm_hash_node *node = NULL;
	struct plm_hash_bucket *bucket;
	
	k = hash->h_key(key, hash->h_bucket_num);
	bucket = &hash->h_bucket[k];
	PLM_LIST_SEARCH(&ln, &bucket->hb_list, value_cmp, value);
	node = (struct plm_hash_node *)ln;
	*pp = node;

	return (node ? 0 : -1);
}

/* delete all values with the same key */
void plm_hash_delete(struct plm_hash *hash, void *key)
{
	uint32_t k;

	k = hash->h_key(key, hash->h_bucket_num);
	hash->h_len -= PLM_LIST_LEN(&hash->h_bucket[k].hb_list);
	PLM_LIST_INIT(&hash->h_bucket[k].hb_list);
}

/* remove the node with specifiy key and value */
void plm_hash_delete2(struct plm_hash *hash, void *key, void *value)
{
	int n;
	struct plm_hash_node *node = NULL;
	uint32_t k = hash->h_key(key, hash->h_bucket_num);
	struct plm_hash_bucket *bucket = &hash->h_bucket[k];
	int old_len = PLM_LIST_LEN(&bucket->hb_list), new_len;

	PLM_LIST_REMOVE_IF(&bucket->hb_list, value_cmp, value);
	hash->h_len -= old_len - new_len;
}
