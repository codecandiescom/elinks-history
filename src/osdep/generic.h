/* $Id: generic.h,v 1.3 2002/09/19 15:49:15 pasky Exp $ */

#ifndef EL__OS_DEPX_H
#define EL__OS_DEPX_H

#ifndef MAXINT
#ifdef INT_MAX
#define MAXINT INT_MAX
#else
#define MAXINT 0x7fffffff
#endif
#endif

#ifndef SA_RESTART
#define SA_RESTART	0
#endif

/*#ifdef sparc
#define htons(x) (x)
#endif*/

#ifndef HAVE_CFMAKERAW
#include <termios.h>
void cfmakeraw(struct termios *t);
#endif

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
