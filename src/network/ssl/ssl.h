/* $Id: ssl.h,v 1.9 2002/07/08 14:52:17 pasky Exp $ */

#ifndef EL__SSL_SSL_H
#define EL__SSL_SSL_H

#ifdef HAVE_SSL
#if !(defined HAVE_OPENSSL) && !(defined HAVE_GNUTLS)
#error "Huh?! You have SSL enabled, but not OPENSSL nor GNUTLS!! And then you want exactly *what* from me?"
#endif
#endif

#ifdef HAVE_SSL
#ifdef HAVE_OPENSSL
#define	ssl_t	SSL
#elif defined(HAVE_GNUTLS)
#define	ssl_t	GNUTLS_STATE
#endif
#else
#define	ssl_t	void
#endif

void init_ssl();
void done_ssl();

ssl_t *get_ssl();
void free_ssl(ssl_t *);

unsigned char *get_ssl_cipher_str(ssl_t *);

#endif
