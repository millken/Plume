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

#ifndef _PLM_EPOLL_H
#define _PLM_EPOLL_H

#include <stdint.h>
#include <sys/epoll.h>

#include "plm_event.h"

#ifdef __cplusplus
extern "C" {
#endif

struct plm_epoll_io {
	struct plm_event_io ei_event_base;
	int ei_efd_global;
	int8_t ei_efd_local_num;
	int ei_efd_local[0];
};

/* init epoll */
int plm_epoll_io_init(struct plm_event_io *e, int maxfd, int thrdn);

/* destroy epoll */
int plm_epoll_io_shutdown(struct plm_event_io *e);

/* epoll wait */
int plm_epoll_io_poll(struct plm_event_io_handler *io, int n,
					  struct plm_event_io *e, int timeout, plm_poller_t t);

/* epoll contrl */
int plm_epoll_io_ctl(struct plm_event_io *e, int fd, int flag, plm_poller_t t);

#ifdef __cplusplus
}
#endif

#endif
