/* $Id: ssl.h,v 1.2 2002/05/08 13:55:06 pasky Exp $ */

#ifndef EL__SSL_H
#define EL__SSL_H

#ifdef HAVE_SSL

/* #include "lowlevel/sched.h" */

void ssl_finish(void);
SSL *getSSL(void);
#endif

#endif
