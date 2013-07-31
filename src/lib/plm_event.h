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

typedef enum {
	PLM_THREAD_LOCAL,
	PLM_PROCESS_GLOBAL
} plm_poller_t;

struct plm_event_io_handler {
	void (*eih_onread)(void *, int);
	void *eih_rddata;
	void (*eih_onwrite)(void *, int);
	void *eih_wrdata;
	int eih_fd;
};	

struct plm_event_io {
	int (*ei_init)(struct plm_event_io *e, int maxfd, int thrdn);
	int (*ei_shutdown)(struct plm_event_io *e);
	int (*ei_poll)(struct plm_event_io_handler *events, int n, 
				   struct plm_event_io *e, int timeout, plm_poller_t t);
	int (*ei_ctl)(struct plm_event_io *e, int fd, int flag, plm_poller_t t);
	struct plm_event_io_handler *ei_events_arr;
};

/* init event driven io
 * @maxfd -- the max number of fd
 * @thrdn -- the number of work thread
 * return 0 -- success, else error
 */
int plm_event_io_init(int maxfd, int thrdn);

/* shutdown the driven io
 * return 0 -- success, else error
 */
int plm_event_io_shutdown();

/* post read event on thread local poller
 * @fd -- file descriptor
 * @data -- the first argument for cb function
 * @handler -- handle event when ready
 * return 0 -- success, else error
 */
int plm_event_io_read(int fd, void *data, void (*handler)(void *, int));

/* post read event on process global poller
 * @fd -- file descriptor
 * @data -- the first argument for cb function
 * @handler -- handle event when ready 
 * return 0 -- success, else error
 */
int plm_event_io_read2(int fd, void *data, void (*handler)(void *, int));	

/* post write event on thread local poller
 * @fd -- file descriptor
 * @data -- the first argument for cb function
 * @handler -- handle event when ready 
 * return 0 -- success, else error
 */
int plm_event_io_write(int fd, void *data, void (*handler)(void *, int));

/* post write event on process global poller
 * @fd -- file descriptor
 * @data -- the first argument for cb function
 * @handler -- handle event when ready  
 * return 0 -- success, else error
 */
int plm_event_io_write2(int fd, void *data, void (*handler)(void *, int));

/* poll event from the current thread poller
 * @evts -- buffer to store events of ready
 * @n -- number of element could store in events buffer 
 * @timeout -- timeout in ms
 * return the number of events of ready, else -1 on error and 0 on timeout
 */
int plm_event_io_poll(struct plm_event_io_handler *evts, int n, int timeout);

/* poll event from the global poller, we use to poll the listen tcp socket
 * @evts -- buffer to store events of ready
 * @n -- number of element could store in events buffer
 * @timeout -- timeout in ms
 * return the number of events of ready, else -1 on error and 0 on timeout
 */	
int plm_event_io_poll2(struct plm_event_io_handler *evts, int n, int timeout);	

#ifdef __cplusplus
}
#endif
	
#endif
