/* $Id: overrides.h,v 1.1 2003/10/27 03:01:38 pasky Exp $ */

#ifndef EL__OSDEP_OVERRIDES_H
#define EL__OSDEP_OVERRIDES_H

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
#ifdef errno
#undef errno
#endif
#define errno 1
#endif

#endif
