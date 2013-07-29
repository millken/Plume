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

#include <unistd.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "plm_plugin_base.h"
#include "plm_threads.h"
#include "plm_event.h"
#include "plm_sync_mech.h"
#include "plm_conf.h"
#include "plm_atomic.h"
#include "plm_log.h"
#include "plm_dispatcher.h"

enum {
	PLM_DISP_RUNNING,
	PLM_DISP_SHUTDOWN,
};

struct plm_disp_global {
	plm_lock_t dg_lock;
	volatile int dg_status;
};

static struct plm_disp_global dg;
static int thrdn;
static int timeout;

pid_t gettid()
{
	return syscall(SYS_gettid);
}

/* start processor threads 
 * return 0 -- sucess, else error
 */
int plm_disp_start()
{
	thrdn = main_ctx.mc_work_thread_num;
	timeout = main_ctx.mc_poll_timeout;
	
	if (thrdn > 1) {
		if (plm_lock_init(&dg.dg_lock))
			return (-1);
	}

	if (!plm_threads_create(thrdn, plm_disp_proc)) {
		plm_threads_run();
		return (0);
	}

	if (thrdn > 1) 
		plm_lock_destroy(&dg.dg_lock);
	
	return (-1);
}

/* shundown and wait for all threads done 
 */
void plm_disp_shutdown()
{
	thrdn = main_ctx.mc_work_thread_num;
	
	dg.dg_status = PLM_DISP_SHUTDOWN;
	plm_threads_end();

	if (thrdn > 1)
		plm_lock_destroy(&dg.dg_lock);
}

#define plm_poll_io(nevs, io, n)				   \
	do {										   \
		if (thrdn > 1)							   \
			plm_lock_lock(&dg.dg_lock);			   \
		nevs = plm_event_io_poll(io, n, timeout);  \
		if (thrdn > 1)							   \
			plm_lock_unlock(&dg.dg_lock);		   \
	} while(0);

static int plm_disp_open_log()
{
	char logfile[MAX_PATH];
	const char *logpath = main_ctx.mc_log_path.s_str;
	int len = main_ctx.mc_log_path.s_len;
	int loglevel = main_ctx.mc_log_level;

	if (len - 10 >= sizeof(logfile)) {
		plm_log_syslog("logfile path too long");
		return (-1);
	}

	memcpy(logfile, logpath, len);
	if (logfile[len-1] != '/')
		logfile[len++] = '/';
	
	sprintf(logfile + len, "plume_%d.log", plm_threads_curr());

	if (plm_log_open(loglevel, logfile)) {
		plm_log_syslog("open %s failed: %s", logfile, strerror(errno));
		return (-1);
	}

	return (0);
}

/* process, main loop
 * never return until shutdown
 */
void plm_disp_proc()
{
	int max = main_ctx.mc_maxfd;
	struct plm_event_io_callback *io_arr;

	if (plm_disp_open_log())
		return;
	
	if (plm_plugin_work_thrd_init()) {
		plm_log_write(PLM_LOG_FATAL, "work thread init hook failed");
		return;
	}

	io_arr = (struct plm_event_io_callback *)
		malloc(max * sizeof(struct plm_event_io_callback));
	
	if (!io_arr) {
		plm_log_write(PLM_LOG_FATAL, "allocate io callback failed");
		plm_log_close();
		return;
	}

	plm_log_write(PLM_LOG_TRACE, "run in thread: %d", gettid());
	
	plm_atomic_test_and_set(&dg.dg_status, PLM_DISP_SHUTDOWN, PLM_DISP_RUNNING);
	for (;;) {
		int i, n;

		/* work thread read status here */
		if (plm_atomic_int_get(&dg.dg_status) != PLM_DISP_RUNNING)
			break;
		
		plm_poll_io(n, io_arr, max);
		for (i = 0; i < n; i++)
			io_arr[i].eic_cb(io_arr[i].eic_data, io_arr[i].eic_fd);
	}

	free(io_arr);
	plm_log_close();
	plm_plugin_work_thrd_destroy();
}

/* notify threads to exit and return immediately */
void plm_disp_notify_exit()
{
	plm_atomic_test_and_set(&dg.dg_status, PLM_DISP_RUNNING, PLM_DISP_SHUTDOWN);
	plm_log_write(PLM_LOG_TRACE, "current status: %d", dg.dg_status);
}
