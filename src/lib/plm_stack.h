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

#ifndef _PLM_STACKE_H
#define _PLM_STACKE_H

#include "plm_list.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef plm_list_t plm_stack_t;
typedef plm_list_node_t plm_stack_node_t;

#define PLM_STACK_INIT(s) PLM_LIST_INIT(s)
#define PLM_STACK_PUSH(s, n) PLM_LIST_ADD_FRONT(s, n)
#define PLM_STACK_POP(s) PLM_LIST_DEL_FRONT(s)
#define PLM_STACK_TOP(pp, s) do { *(pp) = PLM_LIST_FRONT(s); } while (0)
#define PLM_STACK_LEN(p, s) do { *(p) = PLM_LIST_LEN(s); } while (0)
#define PLM_STACK_FOREACH(s, func, data) PLM_LIST_FOREACH(s, func, data)
#define PLM_STACK_SEARCH(pp, s, func, data) \
	PLM_LIST_SEARCH(pp, s, func, data)	

#ifdef __cplusplus
}
#endif

#endif
