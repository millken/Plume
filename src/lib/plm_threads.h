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

#ifndef _PLM_THREAD_H
#define _PLM_THREAD_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* create number of suspend threads
 * @thrdn -- number of threads
 * @proc -- threads proc
 * return 0 -- success, else error
 */
int plm_threads_create(int thrdn, void (*proc)());

/* run threads created before 
 * return 0 -- success, else error
 */
int plm_threads_run();

/* end all threads and wait for them done, called in main thread
 */
void plm_threads_end();

/* notify threads to exit and return immediately */	
void plm_threads_notify_exit();	

/* get the current thread solt
 * return soltn number -- success
 */
int plm_threads_curr();

/* set the current thread cpu affinity with cpu id */
int plm_threads_set_cpu_affinity(uint64_t cpu_mask);	

#ifdef __cplusplus
}
#endif
	
#endif
