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
	void *cf_data;
	void (*cf_handler)(void *);
	char cf_type;
	struct {
		unsigned char cf_open:1;
		unsigned char cf_close:1;
		unsigned char cf_associate:1;
	};
};

struct plm_comm_fd_array {
	int cfa_thread_safe;
	plm_lock_t cfa_lock;

	struct {
		int cfa_inused;
		int cfa_free;
		int cfa_type_inused[PLM_COMM_UDP];
	};
	
	int cfa_bufn;
	struct plm_comm_fd *cfa_buf;
};

static struct plm_comm_fd_array comm_fd_array;
static int plm_comm_open_socket(int type, int port, const char *addr,
								int backlog);

/* init commom stuff
 * @maxfd -- the max number of fd
 * return 0 on success, else error
 */
int plm_comm_init(int maxfd, int thread_safe)
{
	int rc = -1;
	
	if ((thread_safe && !plm_lock_init(&comm_fd_array.cfa_lock))
		|| !thread_safe) {
		comm_fd_array.cfa_buf = (struct plm_comm_fd *)
			malloc(maxfd * sizeof(struct plm_comm_fd));
		if (comm_fd_array.cfa_buf) {
			comm_fd_array.cfa_free = maxfd;
			rc = 0;
		}
		comm_fd_array.cfa_thread_safe = thread_safe;
	}

	return (rc);
}

/* destroy common stuff */
void plm_comm_destroy()
{
	int thread_safe = comm_fd_array.cfa_thread_safe;
	
	if ((thread_safe && !plm_lock_lock(&comm_fd_array.cfa_lock))
		|| !thread_safe) {
		if (comm_fd_array.cfa_buf) {
			free(comm_fd_array.cfa_buf);
			comm_fd_array.cfa_buf = NULL;
		}
		if (thread_safe) {
			plm_lock_unlock(&comm_fd_array.cfa_lock);
			plm_lock_destroy(&comm_fd_array.cfa_lock);
		}
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
 * return a correct fd
 */
int plm_comm_open(int type, const char *path, int flags, int mode,
				  int port, const char *addr, int backlog, int nonblocking)
{
	int fd = -1;
	struct plm_comm_fd *commfd;
	int thread_safe = comm_fd_array.cfa_thread_safe;
	
	switch (type) {
	case PLM_COMM_FILE:
		fd = open(path, flags, mode);
		break;

	case PLM_COMM_TCP:
	case PLM_COMM_UDP:
		fd = plm_comm_open_socket(type, port, addr, backlog);
		break;
	}

	if (fd == -1)
		return (-1);

	if (thread_safe)
		plm_lock_lock(&comm_fd_array.cfa_lock);
	
	comm_fd_array.cfa_inused++;
	comm_fd_array.cfa_free--;
	comm_fd_array.cfa_type_inused[type]++;

	commfd = &comm_fd_array.cfa_buf[fd];
	assert(commfd->cf_open == 0);
	commfd->cf_type = type;
	commfd->cf_open = 1;
	commfd->cf_associate = 0;

	if (thread_safe)
		plm_lock_unlock(&comm_fd_array.cfa_lock);

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
	int type;
	struct plm_comm_fd *commfd = NULL;
	int thread_safe = comm_fd_array.cfa_thread_safe;

	if (thread_safe)
		plm_lock_lock(&comm_fd_array.cfa_lock);

	if (!close(fd)) {
		commfd = &comm_fd_array.cfa_buf[fd];
		assert(commfd->cf_open == 1);
		type = commfd->cf_type;
		commfd->cf_open = 0;
		commfd->cf_associate = 0;
	
		comm_fd_array.cfa_inused--;
		comm_fd_array.cfa_free++;
		comm_fd_array.cfa_type_inused[type]--;
		fd = -1;
	}

	if (thread_safe)
		plm_lock_unlock(&comm_fd_array.cfa_lock);

	if (commfd && commfd->cf_handler)
		commfd->cf_handler(commfd->cf_data);

	return (fd < 0 ? 0 : -1);
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
	int thread_safe = comm_fd_array.cfa_thread_safe;

	assert(comm_fd_array.cfa_buf[fd].cf_type == PLM_COMM_TCP);
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

		if (thread_safe)
			plm_lock_lock(&comm_fd_array.cfa_lock);

		comm_fd_array.cfa_inused++;
		comm_fd_array.cfa_free--;
		comm_fd_array.cfa_type_inused[PLM_COMM_TCP]++;
	
		assert(comm_fd_array.cfa_buf[cfd].cf_open == 0);
		comm_fd_array.cfa_buf[cfd].cf_type = PLM_COMM_TCP;
		comm_fd_array.cfa_buf[cfd].cf_open = 1;

		if (thread_safe)
			plm_lock_unlock(&comm_fd_array.cfa_lock);
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
 * @data -- the first argument for handler
 * @handler -- close handler will be invoked
 * return void
 */
void plm_comm_reg_close_handler(int fd, void *data, void (*handler)(void *))
{
	/* no need to lock */
	comm_fd_array.cfa_buf[fd].cf_data = data;
	comm_fd_array.cfa_buf[fd].cf_handler = handler;
}

int plm_comm_open_socket(int type, int port, const char *addr, int backlog)
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
	comm_fd_array.cfa_buf[fd].cf_associate = added;
}

/* get flag
 * @fd -- a correct fd
 * return flag we have set or not
 */
char plm_comm_get_flag_added(int fd)
{
	return (comm_fd_array.cfa_buf[fd].cf_associate);
}

