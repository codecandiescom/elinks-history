/* $Id: https.h,v 1.1 2002/03/17 17:38:34 pasky Exp $ */

#ifndef EL__PROTOCOL_HTTPS_H
#define EL__PROTOCOL_HTTPS_H

#include <lowlevel/sched.h>

void https_func(struct connection *c);

#ifdef HAVE_SSL
void ssl_finish(void);
SSL *getSSL(void);
#endif

#endif
