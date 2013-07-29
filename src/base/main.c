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
#include <stdint.h>

#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>

#include "plm_string.h"
#include "plm_dispatcher.h"
#include "plm_conf.h"
#include "plm_log.h"
#include "plm_plugin.h"
#include "plm_ctx.h"

static int plm_daemonize();
static int plm_step_signal();
static void plm_get_prefix();
static int plm_doaction(int argc, char *argv[]);
static int plm_pidfile_write();
static void plm_master_proc_loop();
static void plm_worker_proc_loop();

/* not to be daemon process */
int plm_nondaemon;

/* single process mode */
int plm_single_mode;

/* process type */
int plm_proc_type;

/* worker process id */
pid_t worker_pid;

int main(int argc, char *argv[])
{
	int rc;

	plm_get_prefix();

	if (plm_doaction(argc, argv))
		return (0);
	
	if (plm_step_signal()) {
		plm_log_syslog("plume step singal failed");
		return (-1);
	}

	if (!plm_nondaemon) {
		if (plm_daemonize()) {
			plm_log_syslog("plume daemonize failed");
			return (-1);
		}
	}

	if (plm_pidfile_write()) {
		plm_log_syslog("plume open pid file failed");
		return (-1);
	}
	
	if (plm_single_mode) {
		plm_proc_type = 1;
		plm_worker_proc_loop();
	} else {
		/* fork child */
		worker_pid = fork();
		if (worker_pid > 0) {
			plm_log_syslog("fork child: %d", worker_pid);
		} else if (worker_pid == 0) {
			plm_proc_type = 1;
		} else {
			plm_log_syslog("fork child failed: %s", strerror(errno));
			return (-1);
		}
		
		if (plm_proc_type == 0)
			plm_master_proc_loop();

		if (plm_proc_type == 1)
			plm_worker_proc_loop();
	}

	return (0);
}

int plm_daemonize()
{
	int fd;
	pid_t pid;

	pid = fork();
	if (pid < 0)
		return (-1);
	
	if (pid > 0)
		exit(0);

	setsid();
	if (chdir("/") < 0)
		return (-1);

	close(fileno(stdin));
	fd = open("/dev/null", O_RDWR);
	if (fd < 0) {
		plm_log_syslog("open /dev/null failed: %s", strerror(errno));
		return (-1);
	}
	
	dup2(fd, fileno(stdout));
	dup2(fd, fileno(stderr));
	
	return (0);
}

sig_atomic_t plm_chlddone;
sig_atomic_t plm_sigint;
sig_atomic_t plm_sigchld;
sig_atomic_t plm_sigterm;
sig_atomic_t plm_sigquit;
sig_atomic_t plm_sighup;

static void plm_get_chld_status()
{
	pid_t pid;
	int status, i;

RETRY:
	pid = waitpid(worker_pid, &status, 0);
	if (pid == -1) {
		if (errno == EINTR)
			goto RETRY;
		return;
	}

	if (pid == worker_pid) {
		int code;

		if (WIFEXITED(status)) {
			/* worker process exit
			 * because of user signal master SIGHUP to reconfigure
			 * so set plm_chldone ture and clear plm_sighup
			 * then master loop would to fork new worker
			 */
			if (plm_sighup) {
				plm_chlddone = 1;
				plm_sighup = 0;
			} else {
				code = WEXITSTATUS(status);
				plm_log_syslog("child normally exited, "
							   "maybe init failed, so master just exit");
				/* worker process exit normally because of init failed */
				plm_sigquit = 1;
			}

		} else if (WIFSIGNALED(status)) {
			plm_log_syslog("child exit because of signal: %d",
						   WTERMSIG(status));
			plm_chlddone = 1;
		} else {
			if (WIFSTOPPED(status)) {
				plm_log_syslog("child stopped by signal: %d",
							   WSTOPSIG(status));
			} else if (WIFCONTINUED(status)) {
				plm_log_syslog("child resumed by delivery of SIGCONT");
			} else {
				plm_log_syslog("unknown child status, fork new child");
				plm_chlddone = 1;
			}
		}
	}
}

static void plm_sig_handler(int signo)
{
	if (plm_proc_type == 0) {
		switch (signo) {
		case SIGHUP:
			plm_sighup = 1;
			break;
		case SIGQUIT:
			plm_sigquit = 1;
			break;
		case SIGCHLD:
			plm_sigchld = 1;
			break;
		case SIGTERM:
			plm_sigterm = 1;
			break;
		case SIGINT:
			plm_sigint = 1;
			break;
		}
	} else if (plm_proc_type == 1) {
		switch (signo) {
		case SIGQUIT:
		case SIGTERM:
		case SIGINT:
			plm_threads_notify_exit();
			plm_disp_notify_exit();
			break;
		}
	}

	if (plm_sigchld) {
		plm_sigchld = 0;
		plm_get_chld_status();
	}
}

struct plm_sigact {
	int s_signo;
	void (*s_handler)(int signo);
	struct {
		uint8_t s_master_block : 1;
		uint8_t s_worker_block : 1;
	};
};

struct plm_sigact plm_sig[] = {
	{ SIGCHLD, plm_sig_handler, 1, 1 },
	{ SIGQUIT, plm_sig_handler, 1, 0 },
	{ SIGINT, plm_sig_handler, 1, 0 },
	{ SIGTERM, plm_sig_handler, 1, 0 },
	{ SIGHUP, plm_sig_handler, 1, 1 }
};

int plm_step_signal()
{
	int i;

	for (i = 0; i < sizeof(plm_sig) / sizeof(plm_sig[0]); i++) {
		struct sigaction sa;

		memset(&sa, 0, sizeof(sa));
		sa.sa_handler = plm_sig[i].s_handler;
		sigemptyset(&sa.sa_mask);

		if (sigaction(plm_sig[i].s_signo, &sa, NULL) == -1)
			return (-1);
	}

	return (0);
}

static int plm_block_signal_master()
{
	/* block signals until master process signal complete */
	int i;
	sigset_t set;

	sigemptyset(&set);

	for (i = 0; i < sizeof(plm_sig) / sizeof(plm_sig[0]); i++) {
		if (plm_sig[i].s_master_block)
			sigaddset(&set, plm_sig[i].s_signo);
	}

	return sigprocmask(SIG_BLOCK, &set, NULL);
}

static int plm_block_signal_worker()
{
	/* block some signals in worker process */
	int i;
	sigset_t set;

	sigemptyset(&set);

	for (i = 0; i < sizeof(plm_sig) / sizeof(plm_sig[0]); i++) {
		if (plm_sig[i].s_worker_block)
			sigaddset(&set, plm_sig[i].s_signo);
	}

	return sigprocmask(SIG_BLOCK, &set, NULL);	
}

plm_string_t plm_prefix;

void plm_get_prefix()
{
	static char prefix_buf[MAX_PATH];
	plm_prefix.s_len = snprintf(prefix_buf, sizeof(prefix_buf), "%s", PREFIX);
	plm_prefix.s_str = prefix_buf;

	if (prefix_buf[plm_prefix.s_len - 1] != '/')
		prefix_buf[plm_prefix.s_len++] = '/';
}

int plm_pidfile_write()
{
	FILE *fp;
	int rc = -1;
	char filepath[MAX_PATH];

	memcpy(filepath, plm_prefix.s_str, plm_prefix.s_len);
	filepath[plm_prefix.s_len] = 0;
	strcat(filepath, "logs/pid");

	fp = fopen(filepath, "w");
	if (fp) {
		pid_t pid = getpid();
		fprintf(fp, "%d\n", pid);
		fclose(fp);
		rc = 0;
	} else {
		plm_log_syslog("open %s failed: %s", filepath, strerror(errno));
	}

	return (rc);
}

static pid_t plm_pidfile_read()
{
	FILE *fp;
	pid_t pid = -1;
	char filepath[MAX_PATH];

	memcpy(filepath, plm_prefix.s_str, plm_prefix.s_len);
	filepath[plm_prefix.s_len] = 0;
	strcat(filepath, "logs/pid");

	fp = fopen(filepath, "r");
	if (fp) {
		fscanf(fp, "%d\n", &pid);
		fclose(fp);
	} else {
		plm_log_syslog("open %s failed: %s", filepath, strerror(errno));
	}

	return (pid);
}

int plm_kill_master_proc(int signo)
{
	int rc = -1;
	pid_t pid;

	pid = plm_pidfile_read();
	if (pid > 0)
		rc = kill(pid, signo);
	
	return (rc);
}

int plm_doaction(int argc, char *argv[])
{
	int c;
	int dotask = 0;
	
	while ((c = getopt(argc, argv, "s:vSNh?")) != -1) {
		int signo = -1;
		
		switch (c) {
		case 's':
			if (strcmp(optarg, "reconfigure") == 0)
				signo = SIGHUP;
			else if (strcmp(optarg, "quit") == 0)
				signo = SIGQUIT;

			if (signo > 0) {
				int err = plm_kill_master_proc(signo);
				if (err)
					plm_log_syslog("signal master process failed: %d %s",
								   SIGQUIT, strerror(errno));
			} else {
				plm_log_syslog("unknown action");
			}
			dotask++;
			break;
			
		case 'N':
			plm_nondaemon = 1;
			break;
			
		case 'S':
			plm_single_mode = 1;
			break;

		case 'v':
			printf("%s %s\n", PACKAGE_TARNAME, VERSION);
			break;

		case '?':
		case 'h':
		default:
			printf("%s %s\n"
				   "useage: %s [-?hvSN] [-s action]\n"
				   "Options :\n"
				   "  -?,-h : show this help\n"
				   "  -v    : show version and exit\n"
				   "  -S    : run with single process mode\n"
				   "  -N    : run with non daemon process\n"
				   "  -s [reconfigure|quit] : reconfigure or quit \n\n",
				   PACKAGE_TARNAME, VERSION, PACKAGE_TARNAME);
			dotask++;
			break;
		}
	}

	return (dotask);
}

/* returns pid when new work process created */
void plm_master_proc_loop()
{
	sigset_t set;

	if (plm_block_signal_master())
		plm_log_syslog("master block signal failed: %s", strerror(errno));

	sigemptyset(&set);
	
	for (;;) {
		pid_t pid;
		int quit;
		
		/* wait for singal */
		sigsuspend(&set);

		quit = plm_sigquit | plm_sigint | plm_sigterm;

		if (plm_chlddone && !quit) {
			plm_chlddone = 0;
			pid = fork();
			if (pid == 0) {
				/* work process */
				plm_proc_type = 1;
				break;
			} else if (pid > 0) {
				worker_pid = pid;
			} else {
				plm_log_syslog("fork failed: %s", strerror(errno));
			}
		}

		if (quit) {
			kill(worker_pid, SIGQUIT);
			break;
		}

		/* SIGHUP just shutdown worker, then restart it */
		if (plm_sighup == 1) {
			/* just notify one time
			 * the plm_sighup would be clear when worker process done
			 */
			plm_sighup++;
			kill(worker_pid, SIGQUIT);
		}
	}
}

void plm_worker_proc_loop()
{
	int rc;

	if (plm_block_signal_worker())
		plm_log_syslog("worker block signal failed: %s", strerror(errno));

	/* parse configure file and build all plugin context */
	if (plm_conf_load()) {
		plm_log_syslog("plume load configure file failed");
		return;
	}
	
	/* init all plugin with the context */
	rc = plm_plugin_init();
	if (rc < 0) {
		plm_log_syslog("plume init plugin failed");
		return;
	}

	plm_disp_proc();

	/* destroy all plugin and context */
	plm_plugin_destroy();	
	plm_ctx_destroy();
}
