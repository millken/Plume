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

#ifndef _PLM_EVENT_H
#define _PLM_EVENT_H

#ifdef __cplusplus
extern "C" {
#endif

enum {
	PLM_READ = 1,
	PLM_WRITE = 2,
};

struct plm_event_io_callback {
	void (*eic_cb)(void *, int);
	void *eic_data;
	int eic_fd;
};

struct plm_event_io {
	int (*ei_init)(struct plm_event_io *e, int maxfd);
	int (*ei_shutdown)(struct plm_event_io *e);
	int (*ei_poll)(struct plm_event_io_callback *io, int n, 
				   struct plm_event_io *e, int timeout);
	int (*ei_ctl)(struct plm_event_io *e, int fd, int flag);

	int ei_maxfd;
	struct plm_event_io_callback *ei_events_arr;
};

/* init event driven io
 * @maxfd -- the max number of fd   
 * return 0 -- success, else error
 */
int plm_event_io_init(int maxfd);

/* shutdown the driven io
 * return 0 -- success, else error
 */
int plm_event_io_shutdown();

/* read data
 * @fd -- file descriptor
 * @data -- the first argument for cb function
 * @cb -- callback function
 * return 0 -- success, else error
 */
int plm_event_io_read(int fd, void *data, void (*cb)(void *data, int fd));

/* write data
 * @fd -- file descriptor
 * @data -- the first argument for cb function
 * @cb -- callback function
 * return 0 -- success, else error
 */
int plm_event_io_write(int fd, void *data, void (*cb)(void *data, int fd));

/* poll event
 * @io -- store io callback and data
 * @n -- max number of elements in io callback array
 * @timeout -- timeout in ms
 * return the number of events actived, else -1 on error and 0 on timeout
 */
int plm_event_io_poll(struct plm_event_io_callback *io, int n, int timeout);

#ifdef __cplusplus
}
#endif
	
#endif
