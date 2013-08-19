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

#ifndef _PLM_FD_H
#define _PLM_FD_H

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>

#ifdef __cplusplus
extern "C" {
#endif

enum {
	PLM_COMM_FILE = 1,
	PLM_COMM_TCP,
	PLM_COMM_UDP
};

struct plm_comm_close_handler {
	struct plm_comm_close_handler *cch_next;
	void (*cch_handler)(void *);
	void *cch_data;
};	

/* init commom stuff
 * @maxfd -- the max number of fd
 * return 0 on success, else error
 */
int plm_comm_init(int maxfd);

/* destroy common stuff */
void plm_comm_destroy();	

/* create socket or file fd
 * @type -- PLM_COMM_FILE, PLM_COMM_TCP, PLM_COMM_UDP
 * @path -- file path
 * @flags -- flags for file
 * @mode -- mode for file
 * @port -- bind socket with port if we want, -1 indicate ignore
 * @addr -- bind socket with addr if we want, NULL indicate ignore
 * @backlog -- pass to listen
 * @nonblocking -- create a nonblocking fd if set nonblocking to nonzero
 * @reuseaddr -- set SO_REUSEADDR for socket
 * return a correct fd
 */
int plm_comm_open(int type, const char *path, int flags, int mode,
				  int port, const char *addr, int backlog,
				  int nonblocking, int reuseaddr);

/* close the specific fd and call all the close handler
 * @fd -- a corrent fd
 * return 0 -- successful, else failure
 */
int plm_comm_close(int fd);

/* connect to remote
 * @fd -- a correct socket fd
 * @addr -- remote addr
 * @nonblocking -- set nonblocking on new fd
 * return a correct fd on success, -1 on error 
 */
int plm_comm_accept(int fd, struct sockaddr_in *addr, int nonblocking);

/* connect to remote
 * @fd -- a correct socket fd
 * @addr -- remote addr
 * return 0 -- successful, else error code
 */
int plm_comm_connect(int fd, const struct sockaddr_in *addr);

/* read fd and store data in buf
 * @fd -- a correct fd
 * @buf -- buffer for store data
 * @n -- bytes to read
 * return bytes success read or error code
 */
int plm_comm_read(int fd, char *buf, int n);

/* write data to fd
 * @fd -- a correct fd
 * @buf -- the buffer of data to write
 * @n -- bytes to write
 * return bytes success write or error code
 */
int plm_comm_write(int fd, const char *buf, int n);

/* register a close handler
 * @fd -- a correct fd
 * @handler -- close handler
 * return void
 */
void plm_comm_add_close_handler(int fd, struct plm_comm_close_handler *handler);

/* set flag, added indicate we have add the event on fd
 * @fd -- a correct fd
 * @added -- flag
 * return void
 */
void plm_comm_set_flag_added(int fd, char added);

/* get flag
 * @fd -- a correct fd
 * return flag we have set or not
 */
char plm_comm_get_flag_added(int fd);

/* ignore error
 * @err -- errno
 * return 1 -- ture, 0 -- false
 */
int plm_comm_ignore(int err);

#ifdef __cplusplus
}
#endif

#endif
