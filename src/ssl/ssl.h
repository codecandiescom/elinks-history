/* $Id: ssl.h,v 1.4 2002/07/05 01:49:23 pasky Exp $ */

#ifndef EL__SSL_SSL_H
#define EL__SSL_SSL_H

#ifdef HAVE_SSL
#define	ssl_t	SSL
#else
#define	ssl_t	void
#endif

void init_ssl();
void done_ssl();

ssl_t *getSSL();

#endif
