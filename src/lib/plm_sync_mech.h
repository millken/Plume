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

#ifndef _PLM_LOCK_H
#define _PLM_LOCK_H

#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef pthread_mutex_t plm_lock_t;

typedef struct plm_event {
	pthread_mutex_t e_mutex;
	pthread_cond_t e_cond;
} plm_event_t;

#define plm_lock_init(lock) pthread_mutex_init((lock), NULL)
#define plm_lock_destroy(lock) pthread_mutex_destroy((lock))
#define plm_lock_lock(lock) pthread_mutex_lock((lock))
#define plm_lock_trylock(lock) pthread_mutex_trylock((lock))
#define plm_lock_unlock(lock) pthread_mutex_unlock((lock))

/* init an event, event use to wake up 
 * thread which sleep to wait for this event
 * @event -- event object
 * return 0 -- success, else error
 */
int plm_event_init(plm_event_t *event);

/* destroy event object
 * @event -- event object
 * return 0 -- success, else error
 */
int plm_event_destroy(plm_event_t *event);

/* owner or wait for event object
 * @event -- event object
 * return 0 -- success, else error
 */
int plm_event_wait(plm_event_t *event);

/* singal event object, wake up sleep threads
 * @event -- event object
 * return 0 -- success, else error
 */
int plm_event_singal(plm_event_t *event);

#ifdef __cplusplus
}
#endif

#endif
