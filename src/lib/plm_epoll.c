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

#include <stdio.h>
#include <stdlib.h>

#include "plm_comm.h"
#include "plm_epoll.h"

#define PLM_STRUCT_OFFSET(s, m)	(size_t)&(((s *)0)->m)

/* init epoll */
int plm_epoll_io_init(struct plm_event_io *e, int maxfd)
{
	struct plm_epoll_io *ee = (struct plm_epoll_io *)
		((char *)e - PLM_STRUCT_OFFSET(struct plm_epoll_io, ei_event_base));

	ee->ei_epoll_fd = epoll_create(e->ei_maxfd);
	if (ee->ei_epoll_fd > 0) {
		ee->ei_epoll_events = calloc(e->ei_maxfd, sizeof(struct epoll_event));
		if (ee->ei_epoll_events) 
			return (0);
		
		close(ee->ei_epoll_fd);
	}
	return (-1);
}

/* destroy epoll */
int plm_epoll_io_shutdown(struct plm_event_io *e)
{
	struct plm_epoll_io *ee = (struct plm_epoll_io *)
		((char *)e - PLM_STRUCT_OFFSET(struct plm_epoll_io, ei_event_base));

	if (!close(ee->ei_epoll_fd)) {
		free(ee->ei_epoll_events);
		ee->ei_epoll_events = NULL;
		ee->ei_epoll_fd = -1;
	}

	return (ee->ei_epoll_fd == -1 ? 0 : -1);
}

/* epoll wait */
int plm_epoll_io_poll(struct plm_event_io_callback *io, int n, 
					  struct plm_event_io *e, int timeout)
{
	int nevs, i;
	struct plm_epoll_io *ee = (struct plm_epoll_io *)
		((char *)e - PLM_STRUCT_OFFSET(struct plm_epoll_io, ei_event_base));

	nevs = epoll_wait(ee->ei_epoll_fd, ee->ei_epoll_events, n, timeout);
	if (nevs == 0)
		return (0);

	if (nevs < 0)
		return (-1);

	for (i = 0; i < nevs; i++) {
		int fd = ee->ei_epoll_events[i].data.fd;

		io[i].eic_data = e->ei_events_arr[fd].eic_data;
		io[i].eic_cb = e->ei_events_arr[fd].eic_cb;
		io[i].eic_fd = fd;

		e->ei_events_arr[fd].eic_data = NULL;
		e->ei_events_arr[fd].eic_cb = NULL;
	}

	return (nevs);
}

/* epoll contrl */
int plm_epoll_io_ctl(struct plm_event_io *e, int fd, int flag)
{
	int err;
	struct epoll_event ev;
	struct plm_epoll_io *ee = (struct plm_epoll_io *)
		((char *)e - PLM_STRUCT_OFFSET(struct plm_epoll_io, ei_event_base));

	ev.events = 0;
	ev.data.fd = fd;

	if (flag & PLM_READ)
		ev.events = EPOLLIN | EPOLLONESHOT | EPOLLET;
	else if (flag & PLM_WRITE)
		ev.events = EPOLLOUT | EPOLLONESHOT | EPOLLET;

	if (!plm_comm_get_flag_added(fd)) {
		plm_comm_set_flag_added(fd, 1);
		err = epoll_ctl(ee->ei_epoll_fd, EPOLL_CTL_ADD, fd, &ev);
		if (err)
			plm_comm_set_flag_added(fd, 0);
	} else {
		err = epoll_ctl(ee->ei_epoll_fd, EPOLL_CTL_MOD, fd, &ev);
	}

	return (err);
}
