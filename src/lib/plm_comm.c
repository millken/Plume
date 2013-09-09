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

#include "plm_sync_mech.h"
#include "plm_comm.h"

#include <stdlib.h>
#include <assert.h>
#include <errno.h>

struct plm_comm_fd {
	struct plm_comm_close_handler *cf_handler;
	struct {
		unsigned char cf_type:3;
		unsigned char cf_open:1;
		unsigned char cf_close:1;
		unsigned char cf_associate:1;
	};
};

static struct plm_comm_fd *commfd_array;

static int
plm_comm_open_socket(int type, int port, const char *addr,
					 int backlog, int reuseaddr);

/* init commom stuff
 * @maxfd -- the max number of fd
 * return 0 on success, else error
 */
int plm_comm_init(int maxfd)
{
	size_t sz = sizeof(struct plm_comm_fd);
	commfd_array = (struct plm_comm_fd *)malloc(maxfd * sz);
	return (commfd_array ? 0 : -1);
}

/* destroy common stuff */
void plm_comm_destroy()
{
	if (commfd_array) {
		free(commfd_array);
		commfd_array = NULL;
	}
}


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
				  int nonblocking, int reuseaddr)
{
	int fd = -1;
	struct plm_comm_fd *commfd;
	
	switch (type) {
	case PLM_COMM_FILE:
		fd = open(path, flags, mode);
		break;

	case PLM_COMM_TCP:
	case PLM_COMM_UDP:
		fd = plm_comm_open_socket(type, port, addr, backlog, reuseaddr);
		break;
	}

	if (fd == -1)
		return (-1);

	commfd = &commfd_array[fd];
	assert(commfd->cf_open == 0);
	commfd->cf_type = type;
	commfd->cf_open = 1;
	commfd->cf_associate = 0;

	if (nonblocking) {
		int flags = fcntl(fd, F_GETFL, 0);
		fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	}

	return (fd);
}

/* close the specific fd and call all the close handler
 * @fd -- a corrent fd
 * return 0 -- successful, else failure
 */
int plm_comm_close(int fd)
{
	struct plm_comm_fd *commfd = NULL;

	commfd = &commfd_array[fd];
	assert(commfd->cf_open == 1);
	if (commfd && commfd->cf_handler) {
		do {
			struct plm_comm_close_handler *ch;
			void (*handler)(void *);
			void *data;

			ch = commfd->cf_handler;
			handler = ch->cch_handler;
			data = ch->cch_data;
			ch = ch->cch_next;

			handler(data);

			if (!ch)
				break;
		} while (1);
	}

	commfd->cf_open = 0;
	commfd->cf_associate = 0;
	return close(fd);
}

/* connect to remote
 * @fd -- a correct socket fd
 * @addr -- remote addr
 * @nonblocking -- set nonblocking on new fd
 * return a correct fd on success, -1 on error
 */
int plm_comm_accept(int fd, struct sockaddr_in *addr, int nonblocking)
{
	int cfd;
	socklen_t addrlen = sizeof(struct sockaddr_in);

	assert(commfd_array[fd].cf_type == PLM_COMM_TCP);
	cfd = accept(fd, (struct sockaddr *)addr, &addrlen);
	if (cfd > 0) {
		int tmp, flags;

		tmp = flags = fcntl(cfd, F_GETFL, 0);
		if ((flags & O_NONBLOCK) != O_NONBLOCK) {
			if (nonblocking)
				flags |= O_NONBLOCK;
		} else {
			if (!nonblocking)
				flags &= ~(O_NONBLOCK);
		}

		if (tmp != flags)
			fcntl(cfd, F_SETFL, flags);

		assert(commfd_array[cfd].cf_open == 0);
		commfd_array[cfd].cf_type = PLM_COMM_TCP;
		commfd_array[cfd].cf_open = 1;
	}

	return (cfd);
}

/* connect to remote
 * @fd -- a correct socket fd
 * @addr -- remote addr
 * return 0 -- successful, -1 -- on error
 */
int plm_comm_connect(int fd, const struct sockaddr_in *addr)
{
	socklen_t addrlen = sizeof(struct sockaddr_in);
	return connect(fd, (struct sockaddr *)addr, addrlen);
}

/* read fd and store data in buf
 * @fd -- a correct fd
 * @buf -- buffer for store data
 * @n -- bytes to read
 * return bytes success read or error code
 */
int plm_comm_read(int fd, char *buf, int n)
{
	int nr;
TRY:
	nr = read(fd, buf, n);
	if (nr < 0) {
		if (EINTR == errno)
			goto TRY;
	}

	return (nr);
}

/* write data to fd
 * @fd -- a correct fd
 * @buf -- the buffer of data to write
 * @n -- bytes to write
 * return bytes success write or error code
 */
int plm_comm_write(int fd, const char *buf, int n)
{
	int nw;
TRY:
	nw = write(fd, buf, n);
	if (nw < 0) {
		if (EINTR == errno)
			goto TRY;
	}

	return (nw);
}

/* register a close handler
 * @fd -- a correct fd
 * @handler -- close handler
 * return void
 */
void plm_comm_add_close_handler(int fd, struct plm_comm_close_handler *handler)
{
	handler->cch_next = commfd_array[fd].cf_handler;
	commfd_array[fd].cf_handler = handler;
}

int plm_comm_open_socket(int type, int port, const char *addr,
						 int backlog, int reuseaddr)
{
	int sock_type, fd;
	struct sockaddr_in addrin;

	switch (type) {
	case PLM_COMM_TCP:
		sock_type = SOCK_STREAM;
		break;
	case PLM_COMM_UDP:
		sock_type = SOCK_DGRAM;
		break;
	}
	
	fd = socket(AF_INET, sock_type, IPPROTO_TCP);
	if (fd < 0)
		return (-1);

	if (reuseaddr) {
		int on = 1;
		if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
			close(fd);
			return (-1);
		}
	}

	if (port > 0) {
		addrin.sin_family = AF_INET;
		addrin.sin_port = htons(port);
		addrin.sin_addr.s_addr = addr ? inet_addr(addr) : INADDR_ANY;

		if (bind(fd, (struct sockaddr *)&addrin, sizeof(addrin))) {
			close(fd);
			return (-1);
		}
	}

	if (backlog > 0) {
		if (listen(fd, backlog)) {
			close(fd);
			return (-1);
		}
	}

	return (fd);
}


/* set flag, added indicate we have add the event on fd
 * @fd -- a correct fd
 * @added -- flag
 * return void
 */
void plm_comm_set_flag_added(int fd, char added)
{
	commfd_array[fd].cf_associate = added;
}

/* get flag
 * @fd -- a correct fd
 * return flag we have set or not
 */
char plm_comm_get_flag_added(int fd)
{
	return (commfd_array[fd].cf_associate);
}

/* check errno
 * @err -- errno
 * return non-zero when errno could be ignored
 */	
int plm_comm_ignore(int err)
{
	int n;

	switch (err) {
	case EWOULDBLOCK:
#if EAGAIN != EWOULDBLOCK		
	case EAGAIN:
#endif		
	case EINTR:
#ifdef ERESTART
	case ERESTART:
#endif		
	case EALREADY:
	case EINPROGRESS:
		n = 1;

	default:
		n = 0;
	}

	return (n);
}
