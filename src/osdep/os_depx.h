/* $Id: os_depx.h,v 1.7 2003/10/23 22:37:41 pasky Exp $ */

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

/*#ifdef sparc
#define htons(x) (x)
#endif*/

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

#endif
