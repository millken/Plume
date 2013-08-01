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

#ifndef _PLM_TIMER_H
#define _PLM_TIMER_H

#ifdef __cplusplus
extern "C" {
#endif

/* init timer list
 * @thrdn -- number of thread
 * return zero on success, else -1
 */
int plm_timer_init(int thrdn);

/* destroy timer list */	
void plm_timer_destroy();	

/* add a timer in the current thread timer list
 * @handler -- handle to event when time expire
 *             a handler must be return nonzero value
 *             when the handler is time consuming
 * @data -- pass to handler
 * @delta -- delta time in ms
 * return zero on success, else -1
 */
int plm_timer_add(int (*handler)(void *), void *data, int delta);

/* delete a timer from the current thread timer list */
void plm_timer_del(int (*handler)(void *), void *data);

/* check thread timer list
 * return immediately if no timer expire
 * the return value is delta value in ms when the  next timer expire
 */
int plm_timer_run();

#ifdef __cplusplus
}
#endif

#endif
