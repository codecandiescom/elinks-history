/* BeOS system-specific routines. */
/* $Id: beos.c,v 1.8 2003/10/27 01:24:24 pasky Exp $ */

/* Note that this file is currently unmaintained and basically dead. Noone
 * cares about BeOS support, apparently. This file may yet survive for some
 * time, but it will probably be removed if noone will adopt it. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef __BEOS__

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <be/kernel/OS.h>

#include "elinks.h"

#include "osdep/os_depx.h"
#include "terminal/terminal.h"
#include "util/lists.h"


/* Misc trivial stuff */

int
is_xterm(void)
{
	return 0;
}

int
get_system_env(void)
{
	int env = get_common_env();
	unsigned char *term = getenv("TERM");

	if (!term || (upcase(term[0]) == 'B' && upcase(term[1]) == 'E'))
		env |= ENV_BE;

	return env;
}

void
open_in_new_be(struct terminal *term, unsigned char *exe_name,
	       unsigned char *param)
{
	exec_new_elinks(term, DEFAULT_BEOS_TERM_CMD, exe_name, param);
}


/* Threads */

int thr_sem_init = 0;
sem_id thr_sem;

struct active_thread {
	LIST_HEAD(struct active_thread);

	thread_id tid;
	void (*fn)(void *);
	void *data;
};

INIT_LIST_HEAD(active_threads);

int32
started_thr(void *data)
{
	struct active_thread *thrd = data;

	thrd->fn(thrd->data);
	if (acquire_sem(thr_sem) < B_NO_ERROR) return 0;
	del_from_list(thrd);
	free(thrd);
	release_sem(thr_sem);

	return 0;
}

int
start_thr(void (*fn)(void *), void *data, unsigned char *name)
{
	struct active_thread *thrd;
	int tid;

	if (!thr_sem_init) {
		thr_sem = create_sem(0, "thread_sem");
		if (thr_sem < B_NO_ERROR) return -1;
		thr_sem_init = 1;
	} else if (acquire_sem(thr_sem) < B_NO_ERROR) return -1;

	thrd = malloc(sizeof(struct active_thread));
	if (!thrd) goto rel;
	thrd->fn = fn;
	thrd->data = data;
	thrd->tid = spawn_thread(started_thr, name, B_NORMAL_PRIORITY, thrd);
	tid = thrd->tid;

	if (tid < B_NO_ERROR) {
		free(thrd);

rel:
		release_sem(thr_sem);
		return -1;
	}

	resume_thread(thrd->tid);
	add_to_list(active_threads, thrd);
	release_sem(thr_sem);

	return tid;
}

void
terminate_osdep(void)
{
	struct list_head *p;
	struct active_thread *thrd;

	if (acquire_sem(thr_sem) < B_NO_ERROR) return;
	foreach (thrd, active_threads) kill_thread(thrd->tid);

	while ((p = active_threads.next) != &active_threads) {
		del_from_list(p);
		free(p);
	}
	release_sem(thr_sem);
}

int
start_thread(void (*fn)(void *, int), void *ptr, int l)
{
	int p[2];
	struct tdata *t;

	if (c_pipe(p) < 0) return -1;

	t = malloc(sizeof(struct tdata) + l);
	if (!t) return -1;
	t->fn = fn;
	t->h = p[1];
	memcpy(t->data, ptr, l);
	if (start_thr((void (*)(void *))bgt, t, "elinks_thread") < 0) {
		close(p[0]);
		close(p[1]);
		mem_free(t);
		return -1;
	}

	return p[0];
}

int ihpipe[2];

int inth;

void
input_handle_th(void *p)
{
	char c;
	int b = 0;

	setsockopt(ihpipe[1], SOL_SOCKET, SO_NONBLOCK, &b, sizeof(b));
	while (1) if (read(0, &c, 1) == 1) be_write(ihpipe[1], &c, 1);
}

int
get_input_handle(void)
{
	static int h = -1;

	if (h >= 0) return h;
	if (be_pipe(ihpipe) < 0) return -1;
	if ((inth = start_thr(input_handle_th, NULL, "input_thread")) < 0) {
		closesocket(ihpipe[0]);
		closesocket(ihpipe[1]);
		return -1;
	}
	return h = ihpipe[0];
}

void
block_stdin(void)
{
	suspend_thread(inth);
}

void
unblock_stdin(void)
{
	resume_thread(inth);
}

#if 0
int ohpipe[2];

#define O_BUF	16384

void
output_handle_th(void *p)
{
	char *c = malloc(O_BUF);
	int r, b = 0;

	if (!c) return;
	setsockopt(ohpipe[1], SOL_SOCKET, SO_NONBLOCK, &b, sizeof(b));
	while ((r = be_read(ohpipe[0], c, O_BUF)) > 0) write(1, c, r);
	free(c);
}

int
get_output_handle(void)
{
	static int h = -1;

	if (h >= 0) return h;
	if (be_pipe(ohpipe) < 0) return -1;
	if (start_thr(output_handle_th, NULL, "output_thread") < 0) {
		closesocket(ohpipe[0]);
		closesocket(ohpipe[1]);
		return -1;
	}
	return h = ohpipe[1];
}
#endif


#if defined(HAVE_SETPGID)

int
exe(unsigned char *path)
{
	int p = fork();

	if (!p) {
		setpgid(0, 0);
		system(path);
		_exit(0);
	}

	if (p > 0) {
		int s;

		waitpid(p, &s, 0);
	} else
		return system(path);

	return 0;
}

#endif




/* This is the mess of own BeOS-specific wrappers to standard stuff. */

#define SHS	128

int
be_read(int s, void *ptr, int len)
{
	if (s >= SHS) return recv(s - SHS, ptr, len, 0);
	return read(s, ptr, len);
}

int
be_write(int s, void *ptr, int len)
{
	if (s >= SHS) return send(s - SHS, ptr, len, 0);
	return write(s, ptr, len);
}

int
be_close(int s)
{
	if (s >= SHS) return closesocket(s - SHS);
	return close(s);
}

int
be_socket(int af, int sock, int prot)
{
	int h = socket(af, sock, prot);

	if (h < 0) return h;
	return h + SHS;
}

int
be_connect(int s, struct sockaddr *sa, int sal)
{
	return connect(s - SHS, sa, sal);
}

int
be_getpeername(int s, struct sockaddr *sa, int *sal)
{
	return getpeername(s - SHS, sa, sal);
}

int
be_getsockname(int s, struct sockaddr *sa, int *sal)
{
	return getsockname(s - SHS, sa, sal);
}

int
be_listen(int s, int c)
{
	return listen(s - SHS, c);
}

int
be_accept(int s, struct sockaddr *sa, int *sal)
{
	int a = accept(s - SHS, sa, sal);

	if (a < 0) return -1;
	return a + SHS;
}

int
be_bind(int s, struct sockaddr *sa, int sal)
{
#if 0
	struct sockaddr_in *sin = (struct sockaddr_in *)sa;

	if (!sin->sin_port) {
		int i;
		for (i = 16384; i < 49152; i++) {
			sin->sin_port = i;
			if (!be_bind(s, sa, sal)) return 0;
		}
		return -1;
	}
#endif
	if (bind(s - SHS, sa, sal)) return -1;
	getsockname(s - SHS, sa, &sal);
	return 0;
}

#define PIPE_RETRIES	10

int
be_pipe(int *fd)
{
	int s1, s2, s3, l;
	struct sockaddr_in sa1, sa2;
	int retry_count = 0;

again:
	s1 = be_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s1 < 0) {
		/*perror("socket1");*/
		goto fatal_retry;
	}

	s2 = be_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (s2 < 0) {
		/*perror("socket2");*/
		be_close(s1);
		goto fatal_retry;
	}
	memset(&sa1, 0, sizeof(sa1));
	sa1.sin_family = AF_INET;
	sa1.sin_port = 0;
	sa1.sin_addr.s_addr = INADDR_ANY;
	if (be_bind(s1, (struct sockaddr *)&sa1, sizeof(sa1))) {
		/*perror("bind");*/

clo:
		be_close(s1);
		be_close(s2);
		goto fatal_retry;
	}
	if (be_listen(s1, 1)) {
		/*perror("listen");*/
		goto clo;
	}
	if (be_connect(s2, (struct sockaddr *)&sa1, sizeof(sa1))) {
		/*perror("connect");*/
		goto clo;
	}
	l = sizeof(sa2);
	if ((s3 = be_accept(s1, (struct sockaddr *)&sa2, &l)) < 0) {
		/*perror("accept");*/
		goto clo;
	}
	be_getsockname(s3, (struct sockaddr *)&sa1, &l);
	if (sa1.sin_addr.s_addr != sa2.sin_addr.s_addr) {
		be_close(s3);
		goto clo;
	}
	be_close(s1);
	fd[0] = s2;
	fd[1] = s3;
	return 0;

fatal_retry:
	if (++retry_count > PIPE_RETRIES) return -1;
	sleep(1);
	goto again;
}

int
be_select(int n, struct fd_set *rd, struct fd_set *wr, struct fd_set *exc,
	  struct timeval *tm)
{
	int i, s;
	struct fd_set d, rrd;

	FD_ZERO(&d);
	if (!rd) rd = &d;
	if (!wr) wr = &d;
	if (!exc) exc = &d;
	if (n >= FDSETSIZE) n = FDSETSIZE;
	FD_ZERO(exc);
	for (i = 0; i < n; i++) if ((i < SHS && FD_ISSET(i, rd)) || FD_ISSET(i, wr)) {
		for (i = SHS; i < n; i++) FD_CLR(i, rd);
		return MAXINT;
	}
	FD_ZERO(&rrd);
	for (i = SHS; i < n; i++) if (FD_ISSET(i, rd)) FD_SET(i - SHS, &rrd);
	if ((s = select(FD_SETSIZE, &rrd, &d, &d, tm)) < 0) {
		FD_ZERO(rd);
		return 0;
	}
	FD_ZERO(rd);
	for (i = SHS; i < n; i++) if (FD_ISSET(i - SHS, &rrd)) FD_SET(i, rd);
	return s;
}

#ifndef SO_ERROR
#define SO_ERROR	10001
#endif

int
be_getsockopt(int s, int level, int optname, void *optval, int *optlen)
{
	if (optname == SO_ERROR && *optlen >= sizeof(int)) {
		*(int *)optval = 0;
		*optlen = sizeof(int);
		return 0;
	}
	return -1;
}

#endif
