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
#include <string.h>
#include <errno.h>

#include "plm_log.h"
#include "plm_event.h"
#include "plm_epoll.h"

static struct plm_event_io *e;
static int plm_platform_event_io_init(struct plm_event_io **pp, int thrdn);
static int plm_platform_event_io_destroy(struct plm_event_io *p);

/* init event driven io
 * @maxfd -- the max number of fd
 * @thrdn -- the number of thread
 * return 0 -- success, else error
 */
int plm_event_io_init(int maxfd, int thrdn)
{
	size_t sz = sizeof(struct plm_event_io_handler);
	
	/* platform implemetion for init of plm_event structure */
	if (plm_platform_event_io_init(&e, thrdn)) {
		plm_log_write(PLM_LOG_TRACE, "platform event init failed");
		return (-1);
	}

	e->ei_events_arr = calloc(maxfd, sz);
	if (!e->ei_events_arr) {
		plm_log_write(PLM_LOG_TRACE, "alloc memory for event arrary failed");
		return (-1);
	}

	return e->ei_init(e, maxfd, thrdn);
}

/* shutdown the driven io
 * return 0 -- success, else error
 */
int plm_event_io_shutdown()
{
	int err;

	plm_log_write(PLM_LOG_TRACE, "event shutdown");

	err = e->ei_shutdown(e);
	if (err) {
		if (e->ei_events_arr) {
			free(e->ei_events_arr);
			e->ei_events_arr = NULL;
		}

		plm_platform_event_io_destroy(e);
		e = NULL;
	}
	return (err);
}

/* post read event on thread local poller
 * @fd -- file descriptor
 * @data -- the first argument for cb function
 * @handler -- handle event when ready
 * return 0 -- success, else error
 */
int plm_event_io_read(int fd, void *data, void (*handler)(void *, int))
{
	e->ei_events_arr[fd].eih_onread = handler;
	e->ei_events_arr[fd].eih_rddata = data;
	return e->ei_ctl(e, fd, PLM_READ, PLM_THREAD_LOCAL);
}

/* post read event on process global poller
 * @fd -- file descriptor
 * @data -- the first argument for cb function
 * @handler -- handle event when ready  
 * return 0 -- success, else error
 */
int plm_event_io_read2(int fd, void *data, void (*handler)(void *, int))
{
	e->ei_events_arr[fd].eih_onread = handler;
	e->ei_events_arr[fd].eih_rddata = data;
	return e->ei_ctl(e, fd, PLM_READ, PLM_PROCESS_GLOBAL);
}

/* post write event on thread local poller
 * @fd -- file descriptor
 * @data -- the first argument for cb function
 * @handler -- handle event when ready   
 * return 0 -- success, else error
 */
int plm_event_io_write(int fd, void *data, void (*handler)(void *, int))
{
	e->ei_events_arr[fd].eih_onwrite = handler;
	e->ei_events_arr[fd].eih_wrdata = data;
	return e->ei_ctl(e, fd, PLM_WRITE, PLM_THREAD_LOCAL);
}

/* post write event on process global poller
 * @fd -- file descriptor
 * @data -- the first argument for cb function
 * @handler -- handle event when ready   
 * return 0 -- success, else error
 */
int plm_event_io_write2(int fd, void *data, void (*handler)(void *, int))
{
	e->ei_events_arr[fd].eih_onwrite = handler;
	e->ei_events_arr[fd].eih_wrdata = data;
	return e->ei_ctl(e, fd, PLM_WRITE, PLM_PROCESS_GLOBAL);
}

/* poll event from the current thread poller
 * @evts -- buffer to store events of ready
 * @n -- number of element could store in events buffer 
 * @timeout -- timeout in ms
 * return the number of events of ready, else -1 on error and 0 on timeout
 */
int plm_event_io_poll(struct plm_event_io_handler *evts, int n, int timeout)
{
	static int failed_times;
	int nevs;
	
	nevs = e->ei_poll(evts, n, e, timeout, PLM_THREAD_LOCAL);
	if (nevs == -1) {
		plm_log_write(PLM_LOG_FATAL, "thread poll error, failed times=%d, %s",
					  ++failed_times, strerror(errno));
	}

	return (nevs);
}

/* poll event from the global poller, we use to poll the listen tcp socket
 * @evts -- buffer to store events of ready
 * @n -- number of element could store in events buffer
 * @timeout -- timeout in ms
 * return the number of events of ready, else -1 on error and 0 on timeout
 */	
int plm_event_io_poll2(struct plm_event_io_handler *evts, int n, int timeout)
{
	static int failed_times;
	int nevs;
	
	nevs = e->ei_poll(evts, n, e, timeout, PLM_PROCESS_GLOBAL);
	if (nevs == -1) {
		plm_log_write(PLM_LOG_FATAL, "process poll error, failed times=%d, %s",
					  ++failed_times, strerror(errno));
	}

	return (nevs);
}

#define PLM_STRUCT_OFFSET(s, m)	(size_t)&(((s *)0)->m)

int plm_platform_event_io_init(struct plm_event_io **pp, int thrdn)
{
	struct plm_epoll_io *p = (struct plm_epoll_io *)
		malloc(sizeof(struct plm_epoll_io) + sizeof(int) * thrdn);

	p->ei_efd_global = -1;
	p->ei_efd_local_num = thrdn;
	memset(p->ei_efd_local, -1, thrdn * sizeof(int));
	
	*pp = &p->ei_event_base;
	(*pp)->ei_init = plm_epoll_io_init;
	(*pp)->ei_shutdown = plm_epoll_io_shutdown;
	(*pp)->ei_poll = plm_epoll_io_poll;
	(*pp)->ei_ctl = plm_epoll_io_ctl;

	return (0);
}

int plm_platform_event_io_destroy(struct plm_event_io *p)
{
	struct plm_epoll_io *ep = (struct plm_epoll_io *)
		((char *)p - PLM_STRUCT_OFFSET(struct plm_epoll_io, ei_event_base));
	free(ep);
	return (0);
}
