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

#ifndef _PLM_LIST_H
#define _PLM_LIST_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct plm_list_node plm_list_node_t;
typedef struct plm_list plm_list_t;
	
struct plm_list_node {
	plm_list_node_t *ln_next;
};

struct plm_list {
	plm_list_node_t *l_head;
	int l_len;
};

#define PLM_LIST_FRONT(list)(list)->l_head
#define PLM_LIST_LEN(list)	(list)->l_len
#define PLM_LIST_NEXT(node)	(node)->ln_next

/* init single link list */
#define PLM_LIST_INIT(list)		\
	do {						\
		(list)->l_head = NULL;	\
		(list)->l_len = 0;		\
	} while (0)

/* add a node at the front of list */
#define PLM_LIST_ADD_FRONT(list, node)	\
	do {								\
		plm_list_node_t **_head;		\
		_head = &(list)->l_head;		\
		(node)->ln_next = *_head;		\
		*_head = (node);				\
		(list)->l_len++;				\
	} while (0)

#define PLM_LIST_INSERT_BACK(l, old, new)	\
	do {									\
		(new)->ln_next = (old)->ln_next;	\
		(old)->ln_next = (new);				\
		(l)->l_len++;						\
	} while (0)

/* delete the head node */
#define PLM_LIST_DEL_FRONT(list)	\
	do {							\
		plm_list_node_t **_head;	\
		if ((list)->l_len == 0)		\
			break;					\
		_head = &(list)->l_head;	\
		*_head = (*_head)->ln_next;	\
		(list)->l_len--;			\
	} while (0)

/* remove a node from list
 * O(n)
 */
#define PLM_LIST_REMOVE(list, node)		\
	do {								\
		plm_list_node_t **_pp;			\
		for (_pp = &(list)->l_head; *_pp;			\
			 *_pp ? _pp = &(*_pp)->ln_next : 0) {	\
			if ((node) == *_pp) {		\
				*_pp = (node)->ln_next;	\
				(list)->l_len--;		\
			}							\
		}								\
	} while (0)

/* remove a node from list if matched with:
 * int func(plm_list_node_t *node, void *data)
 * func return 1 indicate matched, else return 0
 * O(n)
 */
#define PLM_LIST_REMOVE_IF(list, func, data)	\
	do {										\
		plm_list_node_t **_pp;					\
		for (_pp = &(list)->l_head; *_pp;			\
			 *_pp ? _pp = &(*_pp)->ln_next : 0) {	\
			if (func(*_pp, data)) {			\
				*_pp = (*_pp)->ln_next;		\
				(list)->l_len--;			\
			}								\
		}									\
	} while (0)

/* foreach the list with:
 * func(plm_list_node_t *node, void *data) 
 */
#define PLM_LIST_FOREACH(list, func, data)	\
	do {									\
		plm_list_node_t *_n;				\
		for (_n = (list)->l_head; _n; _n = _n->ln_next) {	\
			func(_n, data);					\
		}									\
	} while (0)

/* foreach the list if condition set up 
 * func(plm_list_node_t *node, void *data) 
 */
#define PLM_LIST_FOREACH_IF(list, func, data, ifc)	\
	do {											\
		plm_list_node_t *_n;						\
		for (_n = (list)->l_head; _n; _n = _n->ln_next) {	\
			if (ifc(data))								\
				func(_n, data);							\
		}												\
	} while (0)

/* search node with:
 * int func(plm_list_node_t *node, void *data)
 * func return 1 indicate matced, else return 0
 */
#define PLM_LIST_SEARCH(pp, list, func, data)	\
	do {										\
		plm_list_node_t *_n;								\
		for (_n = (list)->l_head; _n; _n = _n->ln_next) {	\
			if (func(_n, data)) {							\
				*(pp) = _n;						\
				break;							\
			}									\
		}										\
	} while (0)

#ifdef __cplusplus
}
#endif

#endif
