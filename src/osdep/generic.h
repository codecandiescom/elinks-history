/* $Id: generic.h,v 1.10 2003/10/27 02:49:02 pasky Exp $ */

#ifndef EL__OS_DEPX_H
#define EL__OS_DEPX_H

#ifdef HAVE_LIMITS_H
#include <limits.h> /* may contain PIPE_BUF definition on some systems */
#endif

#ifndef MAXINT
#ifdef INT_MAX
#define MAXINT INT_MAX
#else
#define MAXINT 0x7fffffff
#endif
#endif

#ifndef MAXLONG
#ifdef LONG_MAX
#define MAXLONG LONG_MAX
#else
#define MAXLONG 0x7fffffff
#endif
#endif

#ifndef SA_RESTART
#define SA_RESTART	0
#endif

#ifndef PIPE_BUF
#define PIPE_BUF	512 /* POSIX says that. -- Mikulas */
#endif

/* We define own cfmakeraw() wrapper because cfmakeraw() is broken on AIX,
 * thus we fix it right away. We can also emulate cfmakeraw() if it is not
 * available at all. Face it, we are just cool. */
#include <termios.h>
void elinks_cfmakeraw(struct termios *t);

#ifdef BEOS
#define socket be_socket
#define connect be_connect
#define getpeername be_getpeername
#define getsockname be_getsockname
#define listen be_listen
#define accept be_accept
#define bind be_bind
#define pipe be_pipe
#define read be_read
#define write be_write
#define close be_close
#define select be_select
#define getsockopt be_getsockopt
#ifndef PF_INET
#define PF_INET AF_INET
#endif
#ifndef SO_ERROR
#define SO_ERROR	10001
#endif
#ifdef errno
#undef errno
#endif
#define errno 1
#endif

/* Attempt to workaround the EINTR mess. */
#ifdef EINTR

#ifdef TEMP_FAILURE_RETRY	/* GNU libc */
#define safe_read(fd, buf, count) TEMP_FAILURE_RETRY(read(fd, buf, count))
#define safe_write(fd, buf, count) TEMP_FAILURE_RETRY(write(fd, buf, count))
#else /* TEMP_FAILURE_RETRY */

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

static inline ssize_t
safe_read(int fd, void *buf, size_t count) {
	do {
		register int r = read(fd, buf, count);

		if (r == -1 && errno == EINTR) continue;
		return r;
	} while (1);
}

static inline ssize_t
safe_write(int fd, const void *buf, size_t count) {
	do {
		register int w = write(fd, buf, count);

		if (w == -1 && errno == EINTR) continue;
		return w;
	} while (1);
}
#endif /* TEMP_FAILURE_RETRY */

#else /* EINTR */

#define safe_read(fd, buf, count) read(fd, buf, count)
#define safe_write(fd, buf, count) write(fd, buf, count)

#endif /* EINTR */


#endif
