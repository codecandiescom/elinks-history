/* $Id: https.h,v 1.3 2002/03/18 15:14:54 pasky Exp $ */

#ifndef EL__PROTOCOL_HTTPS_H
#define EL__PROTOCOL_HTTPS_H

#include <lowlevel/sched.h>

void https_func(struct connection *c);

#ifdef HAVE_SSL
void ssl_finish(void);
SSL *getSSL(void);
#endif

#endif
