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
#include <string.h>

#include "plm_comm.h"
#include "plm_epoll.h"

#define PLM_STRUCT_OFFSET(s, m)	(size_t)&(((s *)0)->m)

extern __thread int curr_slot;

/* init epoll */
int plm_epoll_io_init(struct plm_event_io *e, int maxfd, int thrdn)
{
	struct plm_epoll_io *ee = (struct plm_epoll_io *)
		((char *)e - PLM_STRUCT_OFFSET(struct plm_epoll_io, ei_event_base));

	ee->ei_efd_global = epoll_create(maxfd);
	if (ee->ei_efd_global > 0) {
		int i;
				
		for (i = 0; i < thrdn; i++) {
			ee->ei_efd_local[i] = epoll_create(maxfd);
			if (ee->ei_efd_local[i] < 0)
				break;
		}

		if (i >= thrdn)
			return (0);

		/* cleanup */
		for (i = 0; i < thrdn; i++) {
			if (ee->ei_efd_local[i] > 0) {
				close(ee->ei_efd_local[i]);
				ee->ei_efd_local[i] = -1;
			}
		}

		close(ee->ei_efd_global);
		ee->ei_efd_global = -1;
	}
	
	return (-1);
}

/* destroy epoll */
int plm_epoll_io_shutdown(struct plm_event_io *e)
{
	int i;
	struct plm_epoll_io *ee = (struct plm_epoll_io *)
		((char *)e - PLM_STRUCT_OFFSET(struct plm_epoll_io, ei_event_base));

	if (ee->ei_efd_global > 0) {
		if (close(ee->ei_efd_global))
			return (-1);
		
		ee->ei_efd_global = -1;
	}

	for (i = 0; i < ee->ei_efd_local_num; i++) {
		if (ee->ei_efd_local[i] > 0) {
			if (close(ee->ei_efd_local[i]))
				return (-1);
			
			ee->ei_efd_local[i] = -1;
		}
	}

	ee->ei_efd_local_num = 0;
	return (0);
}

/* epoll wait */
int plm_epoll_io_poll(struct plm_event_io_handler *events, int n, 
					  struct plm_event_io *e, int timeout, plm_poller_t t)
{
	int nevs, i, efd;
	struct epoll_event epevts[MAX_EVENTS];	
	struct plm_epoll_io *ee = (struct plm_epoll_io *)
		((char *)e - PLM_STRUCT_OFFSET(struct plm_epoll_io, ei_event_base));

	if (t == PLM_THREAD_LOCAL)
		efd = ee->ei_efd_local[curr_slot];
	else if (t == PLM_PROCESS_GLOBAL)
		efd = ee->ei_efd_global;
	else
		abort();

	nevs = n > MAX_EVENTS ? MAX_EVENTS : n;
	nevs = epoll_wait(efd, epevts, nevs, timeout);
	if (nevs == 0)
		return (0);

	if (nevs < 0)
		return (-1);

	memset(events, 0, nevs * sizeof(events[0]));
	for (i = 0; i < nevs; i++) {
		int fd, epoll_evt;
		struct plm_event_io_handler *eih;

		fd = epevts[i].data.fd;	
		epoll_evt = epevts[i].events;	
		eih = &e->ei_events_arr[fd];

		if (epoll_evt & ~EPOLLOUT && eih->eih_onread) {
			events[i].eih_fd = fd;
			events[i].eih_onread = eih->eih_onread;
			eih->eih_onread = NULL;
			events[i].eih_rddata = eih->eih_rddata;
			eih->eih_rddata = NULL;
		}

		if (epoll_evt & ~EPOLLIN && eih->eih_onwrite) {
			events[i].eih_fd = fd;
			events[i].eih_onwrite = eih->eih_onwrite;
			eih->eih_onwrite = NULL;
			events[i].eih_wrdata = eih->eih_wrdata;
			eih->eih_wrdata = NULL;
		}
	}

	return (nevs);
}

/* epoll contrl */
int plm_epoll_io_ctl(struct plm_event_io *e, int fd, int flag, plm_poller_t t)
{
	int err, efd;
	struct epoll_event ev;
	struct plm_epoll_io *ee = (struct plm_epoll_io *)
		((char *)e - PLM_STRUCT_OFFSET(struct plm_epoll_io, ei_event_base));

	ev.events = 0;
	ev.data.fd = fd;

	if (flag & PLM_READ)
		ev.events = EPOLLIN | EPOLLONESHOT | EPOLLET;
	else if (flag & PLM_WRITE)
		ev.events = EPOLLOUT | EPOLLONESHOT | EPOLLET;
	else
		abort();

	if (t == PLM_THREAD_LOCAL)
		efd = ee->ei_efd_local[curr_slot];
	else if (t == PLM_PROCESS_GLOBAL)
		efd = ee->ei_efd_global;
	else
		abort();

	if (!plm_comm_get_flag_added(fd)) {
		plm_comm_set_flag_added(fd, 1);
		err = epoll_ctl(efd, EPOLL_CTL_ADD, fd, &ev);
		if (err)
			plm_comm_set_flag_added(fd, 0);
	} else {
		err = epoll_ctl(efd, EPOLL_CTL_MOD, fd, &ev);
	}

	return (err);
}
