/* $Id: ssl.h,v 1.1 2002/03/17 23:16:52 pasky Exp $ */

#ifndef EL__SSL_H
#define EL__SSL_H

#ifdef HAVE_SSL

/* #include <lowlevel/sched.h> */

void ssl_finish(void);
SSL *getSSL(void);
#endif

#endif
