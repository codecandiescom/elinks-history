/* $Id: https.h,v 1.3 2002/03/16 22:03:09 pasky Exp $ */

#ifndef EL__HTTPS_H
#define EL__HTTPS_H

#include "sched.h"

void https_func(struct connection *c);

#ifdef HAVE_SSL
void ssl_finish(void);
SSL *getSSL(void);
#endif

#endif
