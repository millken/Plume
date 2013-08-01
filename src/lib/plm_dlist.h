/* Copyright (c) 2011-2013 Xingxing Ke <yykxx@hotmail.com>
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

#ifndef _PLM_DLIST_H
#define _PLM_DLIST_H

/* a double link list implemetion with macro
 * support: add node at head and back, remove head node,
 *			remove node, remove if, search foreach
 * NOTE:
 *		all the routine is macro, be careful with variable name,
 *		there may be some errors trouble you when the macro local 
 *		variable has the same name with outside
 */

#ifdef __cplusplus
extern "C" {
#endif

struct plm_dlist_node {
	struct plm_dlist_node *dn_next;
	struct plm_dlist_node *dn_prev;
};

struct plm_dlist {
	struct plm_dlist_node *d_head;
	struct plm_dlist_node *d_tail;
	int d_len;
};

typedef struct plm_dlist_node plm_dlist_node_t;
typedef struct plm_dlist plm_dlist_t;	

#define PLM_DLIST_FRONT(list)	(list)->d_head
#define PLM_DLIST_TAIL(list)	(list)->d_tail
#define PLM_DLIST_PREV(node)	(node)->dn_prev
#define PLM_DLIST_NEXT(node)	(node)->dn_next
#define PLM_DLIST_LEN(list)		(list)->d_len

/* init a double link list */
#define PLM_DLIST_INIT(list)					\
	do {										\
		(list)->d_head = (list)->d_tail = NULL;	\
		(list)->d_len = 0;						\
	} while (0)

/* add node at front of list */
#define PLM_DLIST_ADD_FRONT(list, node)		\
	do {									\
		(node)->dn_next = (list)->d_head;	\
		(node)->dn_prev = NULL;				\
		if ((list)->d_head)						\
			(list)->d_head->dn_prev = (node);	\
		(list)->d_head = (node);			\
		if (!(list)->d_tail)				\
			(list)->d_tail = (node);		\
		(list)->d_len++;					\
	} while (0)

/* add node at the back of list */
#define PLM_DLIST_ADD_BACK(list, node)		\
	do {									\
		(node)->dn_next = NULL;				\
		(node)->dn_prev = (list)->d_tail;	\
		if ((list)->d_tail)						\
			(list)->d_tail->dn_next = (node);	\
		(list)->d_tail = (node);			\
		if (!(list)->d_head)				\
			(list)->d_head = (node);		\
		(list)->d_len++;					\
	} while (0)

/* remove the head */
#define PLM_DLIST_DEL_FRONT(list)					\
	do {											\
		if ((list)->d_len == 0)						\
			break;									\
		(list)->d_head = (list)->d_head->dn_next;	\
		if ((list)->d_head)							\
			(list)->d_head->dn_prev = NULL;			\
		if ((list)->d_len-- == 1)					\
			(list)->d_tail = NULL;					\
	} while (0)

/* remove the tail */
#define PLM_DLIST_DEL_BACK(list)				\
	do {										\
		if ((list)->d_len == 0)					\
			break;								\
		(list)->d_tail = (list)->d_tail->dn_prev;	\
		if ((list)->d_tail)						\
			(list)->d_tail->dn_next = NULL;		\
		if ((list)->d_len-- == 1)				\
			(list)->d_head = NULL;				\
	} while (0)

#define PLM_DLIST_INSERT_BACK(l, old, new)	\
	do {									\
		(new)->dn_next = (old)->dn_next;	\
		if ((new)->dn_next)					\
			(new)->dn_next->dn_prev = (new);\
		(old)->dn_next = (new);				\
		(new)->dn_prev = (old);				\
		if ((old) == (l)->d_tail)			\
			(l)->d_tail = (new);			\
		(l)->d_len++;						\
	} while (0)

#define PLM_DLIST_INSERT_FRONT(l, old, new)	\
	do {									\
		(new)->dn_next = (old);				\
		if ((old)->dn_prev)					\
			(old)->dn_prev->dn_next = (new);\
		(new)->dn_prev = (old)->dn_prev;	\
		(old)->dn_prev = (new);				\
		if ((old) == (l)->d_head)			\
			(l)->d_head = (new);			\
		(l)->d_len++;						\
	} while (0)


/* search node with:
 * int func(plm_dlist_node_t *node, void *data)
 * func return 1 indicate matched, else 0
 */
#define PLM_DLIST_SEARCH(pp, list, func, data)	\
	do {										\
		plm_dlist_node_t *_curr;										\
		for (_curr = (list)->d_head; _curr; _curr = _curr->dn_next) {	\
			if (func(_curr, (data))) {			\
				*(pp) = _curr;					\
				break;							\
			}									\
		}										\
	} while (0)

/* remove a node from list */
#define PLM_DLIST_REMOVE(list, node)	\
	do {								\
		plm_dlist_node_t *_prev = (node)->dn_prev;	\
		plm_dlist_node_t *_next = (node)->dn_next;	\
		if (_prev)						\
			_prev->dn_next = _next;		\
		if (_next)						\
			_next->dn_prev = _prev;		\
		if ((list)->d_head == (node))	\
			(list)->d_head = _next;		\
		if ((list)->d_tail == (node))	\
			(list)->d_tail = _prev;		\
		(list)->d_len--;				\
	} while (0)

/* remove node if matched with:
 * int func(plm_dlist_node_t *node, void *data)
 * func return 1 indicate matched, else 0
 */
#define PLM_DLIST_REMOVE_IF(list, func, data)	\
	do {										\
		plm_dlist_node_t *_target = NULL;		\
		PLM_DLIST_SEARCH(&_target, list, func, data);	\
		if (_target)									\
			PLM_DLIST_REMOVE(list, _target);			\
	} while (0)

/* foreach the list with:
 * func(plm_dlist_node_t *node, void *data)
 */
#define PLM_DLIST_FOREACH(list, func, data)	\
	do {									\
		plm_dlist_node_t *_n;								\
		for (_n = (list)->d_head; _n; _n = _n->dn_next) {	\
			func(_n, data);					\
		}									\
	} while (0)

#ifdef __cplusplus
}
#endif

#endif
