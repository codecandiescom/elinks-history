/* $Id: ssl.h,v 1.5 2002/07/05 01:54:25 pasky Exp $ */

#ifndef EL__SSL_SSL_H
#define EL__SSL_SSL_H

#ifdef HAVE_SSL
#define	ssl_t	SSL
#else
#define	ssl_t	void
#endif

void init_ssl();
void done_ssl();

ssl_t *get_ssl();
void free_ssl(ssl_t *);

#endif
