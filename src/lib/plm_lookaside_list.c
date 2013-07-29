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

#include <assert.h>
#include <string.h>

#include "plm_lookaside_list.h"

struct plm_lookaside_list_node {
	/* tag in node header per object */
	unsigned int lln_tag;

	/* list node */
	struct plm_list_node lln_node;
};

#define PLM_STRUCT_OFFSET(s, m)	(size_t)&(((s *)0)->m)

/* init a lookaside list, a lookaside list is just like 
 * a memory pool with object which has the same size
 * @list -- list to init
 * @high_level -- high water level, the number of free objects hold in list
 * @obj_sz -- object size in bytes
 * @tag -- tag of node header
 * @alloc -- memory allocator, called when there is no free object in list
 * @mfree -- memory free function, called when the number of free objects
 *           larger than high_level
 * return void
 */
void plm_lookaside_list_init(struct plm_lookaside_list *list, int high_level,
							 size_t obj_sz, unsigned int tag,
							 void *(*alloc)(size_t), void (*mfree)(void *))
{
	list->ll_alloc = alloc;
	list->ll_free = mfree;
	list->ll_high_level = high_level;
	list->ll_obj_sz = obj_sz;
	list->ll_tag = tag;
	memset(&list->ll_misc, 0, sizeof(list->ll_misc));
	memset(&list->ll_onoff, 0, sizeof(list->ll_onoff));
	plm_lock_init(&list->ll_lock);

	PLM_LIST_INIT(&list->ll_list);
}

/* enable feature 
 * @list -- list to enable
 * @zero_memory -- set memory to zero when allocated
 * @tag_check -- check tag and rasie an error when check failed
 * @thrdsafe -- thread safe
 * return void
 */
void plm_lookaside_list_enable(struct plm_lookaside_list *list,
							   int zero_memory, int tag_check, int thrdsafe)
{
	list->ll_onoff.ll_tag_check = tag_check;
	list->ll_onoff.ll_zero_memory = zero_memory;
	list->ll_onoff.ll_thrdsafe = thrdsafe;
}

/* destroy a list, this just free the objects in list 
 * @list -- list to destroy
 * return void
 */
void plm_lookaside_list_destroy(struct plm_lookaside_list *list)
{
	struct plm_lookaside_list_node *obj_hdr;

	if (list->ll_onoff.ll_thrdsafe)
		plm_lock_lock(&list->ll_lock);
	
	while (PLM_LIST_LEN(&list->ll_list) > 0) {
		char *n = (char *)PLM_LIST_FRONT(&list->ll_list);
		PLM_LIST_DEL_FRONT(&list->ll_list);

		obj_hdr = (struct plm_lookaside_list_node *)
			(n - PLM_STRUCT_OFFSET(struct plm_lookaside_list_node, lln_node));
		if (list->ll_onoff.ll_tag_check) {
			if (obj_hdr->lln_tag != list->ll_tag)
				assert(0);
		}

		list->ll_free(obj_hdr);
	}

	if (list->ll_onoff.ll_thrdsafe)
		plm_lock_unlock(&list->ll_lock);

	plm_lock_destroy(&list->ll_lock);
}


/* allocate from lookaside list 
 * @list -- lookaside list
 * @inlock_handler -- handler called after memory allocated and before
 *     relase lock and pass the memory allocated to it as argument
 * return NULL if failed, else return the memory address
 */
void *plm_lookaside_list_alloc(struct plm_lookaside_list *list,
							   void inlock_handler(void *))
{
	char *obj_hdr;

	if (list->ll_onoff.ll_thrdsafe)
		plm_lock_lock(&list->ll_lock);

	if (PLM_LIST_LEN(&list->ll_list) > 0) {
		char *list_node = (char *)PLM_LIST_FRONT(&list->ll_list);
		PLM_LIST_DEL_FRONT(&list->ll_list);
		list->ll_misc.ll_alloc_times_from_list++;

		obj_hdr = list_node
			- PLM_STRUCT_OFFSET(struct plm_lookaside_list_node, lln_node);
		if (list->ll_onoff.ll_tag_check) {
			if (((struct plm_lookaside_list_node *)obj_hdr)->lln_tag
				!= list->ll_tag)
				assert(0);
		}
	} else {
		obj_hdr = (char *)list->ll_alloc
			(list->ll_obj_sz + sizeof(struct plm_lookaside_list_node));
		if (obj_hdr)
			((struct plm_lookaside_list_node *)obj_hdr)->lln_tag = list->ll_tag;
	}

	list->ll_misc.ll_alloc_times++;
	if (!obj_hdr)
		list->ll_misc.ll_alloc_failed_times++;

	if (inlock_handler && obj_hdr)
		inlock_handler(obj_hdr + sizeof(struct plm_lookaside_list_node));

	if (list->ll_onoff.ll_thrdsafe)
		plm_lock_unlock(&list->ll_lock);

	if (list->ll_onoff.ll_zero_memory && obj_hdr)
		memset(obj_hdr + sizeof(struct plm_lookaside_list_node),
			   0, list->ll_obj_sz);

	return (obj_hdr ? obj_hdr + sizeof(struct plm_lookaside_list_node) : NULL);
}

/* free object in to lookaside list 
 * @list -- lookaside list
 * @obj -- object to free
 * @inlock_handler -- handler called before free memory to pool and relase lock
 * return none
 */
void plm_lookaside_list_free(struct plm_lookaside_list *list, void *obj,
							 void inlock_handler(void *))
{
	struct plm_lookaside_list_node *obj_hdr;

	obj_hdr = (struct plm_lookaside_list_node *)
		((char *)obj - sizeof(struct plm_lookaside_list_node));

	if (list->ll_onoff.ll_tag_check) {
		if (obj_hdr->lln_tag != list->ll_tag)
			assert(0);
	}

	if (list->ll_onoff.ll_thrdsafe)
		plm_lock_lock(&list->ll_lock);

	if (inlock_handler)
		inlock_handler(obj);
	
	if (PLM_LIST_LEN(&list->ll_list) > list->ll_high_level) {
		list->ll_free(obj_hdr);
	} else {
		PLM_LIST_ADD_FRONT(&list->ll_list, &obj_hdr->lln_node);
		list->ll_misc.ll_free_times_to_list++;
	}

	list->ll_misc.ll_free_times++;

	if (list->ll_onoff.ll_thrdsafe)
		plm_lock_unlock(&list->ll_lock);
}

