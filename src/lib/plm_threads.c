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

#define _GNU_SOURCE
#include <sched.h>
#include <pthread.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "plm_sync_mech.h"
#include "plm_atomic.h"
#include "plm_log.h"
#include "plm_threads.h"

enum {
	PLM_THRD_INIT,
	PLM_THRD_RUN,
	PLM_THRD_SHUTDOWN
};

struct plm_threads_mgr {
	/* how many threads need to create and number of active threads */
	int tm_thrdn;
	int tm_active_thrdn;

	/* threads manger status */
	int tm_status;

	/* thread proc */
	void (*tm_proc)();

	/* event to wake up threads and do the real job */
	plm_event_t tm_start_event;

	pthread_t *tm_thread_id;
};

__thread int curr_slot = 0;
static struct plm_threads_mgr thrd_mgr;
static int plm_create_platform_thread(pthread_t *thrd_id, void *data);

/* create number of suspend threads
 * @thrdn -- number of threads
 * @proc -- threads proc
 * return 0 -- success, else error
 */
int plm_threads_create(int thrdn, void (*proc)())
{
	size_t i;

	if (plm_event_init(&thrd_mgr.tm_start_event))
		return (-1);

	thrd_mgr.tm_proc = proc;
	thrd_mgr.tm_thrdn = thrdn;
	thrd_mgr.tm_active_thrdn = 0;
	thrd_mgr.tm_status = PLM_THRD_INIT;

	thrd_mgr.tm_thread_id = (pthread_t *)calloc(thrdn, sizeof(pthread_t));
	if (!thrd_mgr.tm_thread_id)
		return (-1);

	for (i = 1; i < thrdn; i++) {
		if (plm_create_platform_thread(&thrd_mgr.tm_thread_id[i], (void *)i))
			plm_log_syslog("plumed create thread failed");
	}

	return (i == thrdn? 0 : -1);
}

/* run threads created before 
 * return 0 -- success, else error
 */
int plm_threads_run()
{
	thrd_mgr.tm_status = PLM_THRD_RUN;

	if (thrd_mgr.tm_thrdn == 0)
		return (0);

	for (;;) {
		int n = plm_atomic_int_get(&thrd_mgr.tm_active_thrdn);
		/* this function called in main thread, and after this call
		 * main thread will be a worker thread
		 */
		if (n >= thrd_mgr.tm_thrdn - 1)
			break;
		plm_event_singal(&thrd_mgr.tm_start_event);
		usleep(100);
	}

	return (0);
}

/* end all threads and wait for them done, called in main thread
 */
void plm_threads_end()
{
	int i;

	thrd_mgr.tm_status = PLM_THRD_SHUTDOWN;
	
	for (i = 1; i < thrd_mgr.tm_thrdn; i++) {
		void *retval;
		pthread_t thrd = thrd_mgr.tm_thread_id[i];

		if (!thrd)
			continue;

		if (pthread_join(thrd, &retval))
			plm_log_write(PLM_LOG_TRACE, "join thread failed: %d", thrd);
		else
			plm_log_write(PLM_LOG_TRACE, "thread %d exit", thrd);
	}

	if (thrd_mgr.tm_thread_id) {
		free(thrd_mgr.tm_thread_id);
		thrd_mgr.tm_thread_id = NULL;
	}

	plm_event_destroy(&thrd_mgr.tm_start_event);
}

/* notify threads to exit and return immediately */	
void plm_threads_notify_exit()
{
	thrd_mgr.tm_status = PLM_THRD_SHUTDOWN;
}

static void *plm_thread_proc(void *data)
{
	size_t currn = (size_t)data;

	plm_event_wait(&thrd_mgr.tm_start_event);
	currn = plm_atomic_int_inc(&thrd_mgr.tm_active_thrdn);
	
	curr_slot = (int)currn;

	for (;;) {
		if (plm_atomic_int_get(&thrd_mgr.tm_status) == PLM_THRD_SHUTDOWN)
			break;

		thrd_mgr.tm_proc();
	}

	plm_atomic_int_dec(&thrd_mgr.tm_active_thrdn);
}

/* get the current thread solt
 * return soltn number -- success
 */
int plm_threads_curr()
{
	return (curr_slot);
}

int plm_create_platform_thread(pthread_t *thrd_id, void *data)
{
	return pthread_create(thrd_id, NULL, plm_thread_proc, data);
}

/* set the current thread cpu affinity with cpu id */
int plm_threads_set_cpu_affinity(uint64_t cpu_mask)
{
	cpu_set_t mask;
	pthread_t thrd;
	int i = 0;

	CPU_ZERO(&mask);

	while (cpu_mask) {
		if (cpu_mask & 1)
			CPU_SET(i, &mask);
		i++;
		cpu_mask >>= 1;
	}
	
	if (curr_slot == 0)
		thrd = pthread_self();
	else
		thrd = thrd_mgr.tm_thread_id[curr_slot];

	return pthread_setaffinity_np(thrd, sizeof(mask), &mask);
}
