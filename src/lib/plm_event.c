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
static int plm_platform_event_io_init(struct plm_event_io **pp);
static int plm_platform_event_io_destroy(struct plm_event_io *p);

/* init event driven io
 * @maxfd -- the max number of fd      
 * return 0 -- success, else error
 */
int plm_event_io_init(int maxfd)
{
	/* platform implemetion for init of plm_event structure */
	if (plm_platform_event_io_init(&e)) {
		plm_log_write(PLM_LOG_TRACE, "platform event init failed");
		return (-1);
	}

	e->ei_maxfd = maxfd;
	e->ei_events_arr = (struct plm_event_io_callback *)
		calloc(e->ei_maxfd, sizeof(struct plm_event_io_callback));
	if (!e->ei_events_arr) {
		plm_log_write(PLM_LOG_TRACE, "allocate memory for event arrary failed");
		return (-1);
	}

	return e->ei_init(e, e->ei_maxfd);
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

/* read data
 * @fd -- file descriptor
 * @data -- the first argument for cb function
 * @cb -- callback function
 * return 0 -- success, else error
 */
int plm_event_io_read(int fd, void *data, void (*cb)(void *data, int fd))
{
	e->ei_events_arr[fd].eic_cb = cb;
	e->ei_events_arr[fd].eic_data = data;
	e->ei_events_arr[fd].eic_fd = fd;
	return e->ei_ctl(e, fd, PLM_READ);
}

/* write data
 * @fd -- file descriptor
 * @data -- the first argument for cb function
 * @cb -- callback function
 * return 0 -- success, else error
 */
int plm_event_io_write(int fd, void *data, void (*cb)(void *data, int fd))
{
	e->ei_events_arr[fd].eic_cb = cb;
	e->ei_events_arr[fd].eic_data = data;
	e->ei_events_arr[fd].eic_fd = fd;
	return e->ei_ctl(e, fd, PLM_WRITE);
}

/* poll event
 * @io -- store io callback and data
 * @n -- max number of elements in io callback array
 * @timeout -- timeout in ms
 * return the number of events actived, else -1 on error and 0 on timeout
 */
int plm_event_io_poll(struct plm_event_io_callback *io, int n, int timeout)
{
	static int failed_times;
	int nevs;
	
	nevs = e->ei_poll(io, n, e, timeout);
	if (nevs == -1)
		plm_log_write(PLM_LOG_FATAL, "poll error, failed times=%d, %s",
					  ++failed_times, strerror(errno));

	return (nevs);
}

#define PLM_STRUCT_OFFSET(s, m)	(size_t)&(((s *)0)->m)

int plm_platform_event_io_init(struct plm_event_io **pp)
{
	struct plm_epoll_io *p = (struct plm_epoll_io *)
		malloc(sizeof(struct plm_epoll_io));

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
