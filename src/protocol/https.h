/* $Id: https.h,v 1.2 2002/03/17 13:54:14 pasky Exp $ */

#ifndef EL__HTTPS_H
#define EL__HTTPS_H

#include <lowlevel/sched.h>

void https_func(struct connection *c);

#ifdef HAVE_SSL
void ssl_finish(void);
SSL *getSSL(void);
#endif

#endif
