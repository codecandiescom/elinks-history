/* $Id: ssl.h,v 1.3 2002/07/05 01:29:10 pasky Exp $ */

#ifndef EL__SSL_SSL_H
#define EL__SSL_SSL_H

#ifdef HAVE_SSL
#define	ssl_t	SSL
#else
#define	ssl_t	void
#endif

void ssl_finish(void);
ssl_t *getSSL(void);

#endif
