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
#include <time.h>

#include "plm_dlist.h"
#include "plm_lookaside_list.h"
#include "plm_timer.h"

#ifndef MAX_TIMERS
#define MAX_TIMERS 128
#endif

struct plm_thread_timer_list {
	struct plm_lookaside_list tl_pool;
	plm_dlist_t tl_tmlist;
};

struct plm_timer_list {
	int ttl_tml_num;
	struct plm_thread_timer_list ttl_tml[0];
};

struct plm_timer_obj {
	plm_dlist_node_t to_node;
	int (*to_handler)(void *);
	void *to_data;
	time_t to_expire;
};

static struct plm_timer_list *tmlist;
time_t current_time_ms;
struct timeval current_timeval;
extern __thread int curr_slot;

static int plm_timer_bigger(void *, void *);
static int plm_timer_equal(void *, void *);
static void plm_timer_update_current();

/* init timer list
 * @thrdn -- number of thread
 * return zero on success, else -1
 */
int plm_timer_init(int thrdn)
{
	size_t size;
	struct plm_timer_list *tl;

	size = sizeof(struct plm_timer_list) +
		thrdn * sizeof(struct plm_thread_timer_list);
	tl = (struct plm_timer_list *)malloc(size);
	if (tl) {
		int i;
		size_t timer_obj_sz = sizeof(struct plm_timer_obj);
		
		tl->ttl_tml_num = thrdn;
		for (i = 0; i < thrdn; i++) {
			PLM_DLIST_INIT(&tl->ttl_tml[i].tl_tmlist);
			plm_lookaside_list_init(&tl->ttl_tml[i].tl_pool, MAX_TIMERS,
									timer_obj_sz, -1, malloc, free);
			plm_lookaside_list_enable(&tl->ttl_tml[i].tl_pool, 0, 0, 0);
		}

		tmlist = tl;
	}

	return (tl ? 0 : -1);
}

/* destroy timer list */	
void plm_timer_destroy()
{
	int i;
	
	if (!tmlist)
		return;

	for (i = 0; i < tmlist->ttl_tml_num; i++)
		plm_lookaside_list_destroy(&tmlist->ttl_tml[i].tl_pool);

	free(tmlist);
	tmlist = NULL;
}

/* add a timer in the current thread timer list
 * @handler -- handle to event when time expire
 *             a handler must be return nonzero value
 *             when the handler is time consuming
 * @data -- pass to handler
 * @delta -- delta time in ms
 * return zero on success, else -1
 */
int plm_timer_add(int (*handler)(void *), void *data, int delta)
{
	struct plm_timer_obj *obj;

	obj = (struct plm_timer_obj *)
		plm_lookaside_list_alloc(&tmlist->ttl_tml[curr_slot].tl_pool, NULL);
	if (obj) {
		plm_dlist_node_t *ptr = NULL;
		plm_dlist_t *list;
		
		obj->to_handler = handler;
		obj->to_data = data;
		obj->to_expire = current_time_ms + delta;

		list = &tmlist->ttl_tml[curr_slot].tl_tmlist;
		PLM_DLIST_SEARCH(&ptr, list, plm_timer_bigger, obj);
		if (ptr)
			PLM_DLIST_INSERT_FRONT(list, ptr, &obj->to_node);
		else
			PLM_DLIST_ADD_BACK(list, &obj->to_node);
	}

	return (obj ? 0 : -1);
}

/* delete a timer from the current thread timer list */
void plm_timer_del(int (*handler)(void *), void *data)
{
	struct plm_timer_obj *obj;
	struct plm_lookaside_list *pool;
	plm_dlist_node_t *node;
	plm_dlist_t *list;

	list = &tmlist->ttl_tml[curr_slot].tl_tmlist;
	pool = &tmlist->ttl_tml[curr_slot].tl_pool;
	PLM_DLIST_SEARCH(&node, list, plm_timer_equal, obj);
	if (node) {
		PLM_DLIST_REMOVE(list, node);
		obj = (struct plm_timer_obj *)node;
		plm_lookaside_list_free(pool, obj, NULL);
	}
}

/* check thread timer list
 * return immediately if no timer expire
 * the return value is delta value in ms when the  next timer expire
 */
int plm_timer_run()
{
	plm_dlist_t *list;
	struct plm_timer_obj *obj;
	struct plm_lookaside_list *pool;

	plm_timer_update_current();

	list = &tmlist->ttl_tml[curr_slot].tl_tmlist;
	pool = &tmlist->ttl_tml[curr_slot].tl_pool;
	if (PLM_DLIST_LEN(list) == 0)
		return (0);

	for (;;) {
		obj = (struct plm_timer_obj *)PLM_DLIST_FRONT(list);
		if (obj->to_expire > current_time_ms)
			break;
		
		PLM_DLIST_DEL_FRONT(list);
		obj->to_handler(obj->to_data);
		plm_lookaside_list_free(pool, obj, NULL);

		if (PLM_DLIST_LEN(list) == 0)
			break;
	}
	
	if (PLM_DLIST_LEN(list) == 0)
		return (0);

	obj = (struct plm_timer_obj *)PLM_DLIST_FRONT(list);
	return (obj->to_expire - current_time_ms);
}

int plm_timer_bigger(void *node, void *data)
{
	struct plm_timer_obj *obj1, *obj2;

	obj1 = (struct plm_timer_obj *)node;
	obj2 = (struct plm_timer_obj *)data;

	return (obj1->to_expire > obj2->to_expire);
}

int plm_timer_equal(void *node, void *data)
{
	struct plm_timer_obj *obj1, *obj2;

	obj1 = (struct plm_timer_obj *)node;
	obj2 = (struct plm_timer_obj *)data;

	return (obj1->to_expire == obj2->to_expire);
}

void plm_timer_update_current()
{
	gettimeofday(&current_timeval, NULL);
	current_time_ms = current_timeval.tv_sec * 1000 +
		current_timeval.tv_usec / 1000;
}
