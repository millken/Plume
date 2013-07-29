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

#ifndef _PLM_LOOKASIDE_LIST_H
#define _PLM_LOOKASIDE_LIST_H

#include "plm_list.h"
#include "plm_sync_mech.h"

#ifdef __cplusplus
extern "C" {
#endif

struct plm_lookaside_list {
	/* use to allocate or free memory from or to system */
	void *(*ll_alloc)(size_t);
	void (*ll_free)(void *);

	/* size in bytes per object */
	size_t ll_obj_sz;

	/* the max number of object in list */
	int ll_high_level;

	/* node tag, use to verify valid object */
	unsigned int ll_tag;

	/* object list */
	struct plm_list ll_list;

	/* misc */
	struct {
		/* how many times allocate */
		long long  ll_alloc_times;

		/* how many times free */
		long long  ll_free_times;

		/* how many times allocate from list */
		long long  ll_alloc_times_from_list;

		/* how many times free to list */
		long long  ll_free_times_to_list;

		/* how many times allocate failed */
		long long  ll_alloc_failed_times;
	} ll_misc;

	/* on/off */
	union {
		unsigned char ll_zero_memory:1;
		unsigned char ll_tag_check:1;
		unsigned char ll_thrdsafe:1;
	} ll_onoff;

	plm_lock_t ll_lock;
};

/* init a lookaside list, a lookaside list is just like 
 * a memory pool with object which has the same size
 * @list -- list to init
 * @high_level -- high water level, the number of free objects hold in list
 * @obj_sz -- object size in bytes
 * @tag -- tag of node header
 * @alloc -- memory allocator, called when there is no free object in list
 * @mfree -- memory free function, called when the number of free objects
 * larger than  high_level
 * return void
 */
void plm_lookaside_list_init(struct plm_lookaside_list *list, int high_level,
							 size_t obj_sz, unsigned int tag,
							 void *(*alloc)(size_t),
							 void (*mfree)(void *));

/* enable feature 
 * @list -- list to enable
 * @zero_memory -- set memory to zero when allocated
 * @tag_check -- check tag and rasie an error when check failed
 * @thrdsafe -- thread safe
 * return void
 */
void plm_lookaside_list_enable(struct plm_lookaside_list *list,
							   int zero_memory, int tag_check, int thrdsafe);

/* destroy a list, this just free the objects in list 
 * @list -- list to destroy
 * return void
 */
void plm_lookaside_list_destroy(struct plm_lookaside_list *list);

/* allocate from lookaside list 
 * @list -- lookaside list
 * @inlock_handler -- handler called after memory allocated and before
 *     relase lock and pass the memory allocated to it as argument
 * return NULL if failed, else return the memory address
 */
void *plm_lookaside_list_alloc(struct plm_lookaside_list *list,
							   void inlock_handler(void *));

/* free object in to lookaside list 
 * @list -- lookaside list
 * @obj -- object to free
 * @inlock_handler -- handler called before free memory to pool and relase lock
 *     and pass obj to it as the argument
 * return none
 */
void plm_lookaside_list_free(struct plm_lookaside_list *list, void *obj,
							 void inlock_handler(void *));

/* get the number of free objects */
#define plm_lookaside_list_free_objects(l, n) \
	do { \
		if ((l)->ll_onoff.ll_thrdsafe) \
			plm_lock_lock(&((l)->ll_lock)); \
		*n = PLM_LIST_LEN(&((l)->ll_list)); \
		if ((l)->ll_onoff.ll_thrdsafe) \
			plm_lock_unlock(&((l)->ll_lock)); \
	} while (0)

#define plm_lookaside_list_dump_misc(l, a, b, c, d, e) \
	do { \
		if ((l)->ll_onoff.ll_thrdsafe)	\
			plm_lock_lock(&((l)->ll_lock)); \
		*a = (l)->ll_misc.ll_alloc_times; \
		*b = (l)->ll_misc.ll_free_times; \
		*c = (l)->ll_misc.ll_alloc_times_from_list; \
		*d = (l)->ll_misc.ll_free_times_to_list; \
		*e = (l)->ll_misc.ll_alloc_failed_times; \
		if ((l)->ll_onoff.ll_thrdsafe) \
			plm_lock_unlock(&((l)->ll_lock)); \
	} while (0)

#ifdef __cplusplus
}
#endif
	
#endif
