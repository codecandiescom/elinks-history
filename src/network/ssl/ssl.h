/* $Id: ssl.h,v 1.11 2003/07/06 21:25:49 pasky Exp $ */

#ifndef EL__SSL_SSL_H
#define EL__SSL_SSL_H

/* XXX: BIG BIG IMPORTANT FAT NOTE! :XXX
 * 
 * The first rule of fight club is:
 * 	You do not include ssl/ssl.h.
 * 
 * The second rule of fight club is:
 * 	You DO NOT include ssl/ssl.h.
 * 
 * The third rule of fight club is:
 * 	If you include util/error.h before ssl/ssl.h, the compilation is over!
 * 
 * The fourth rule of fight club is:
 * 	Only one inclusion of ssl/ssl.h at once.
 * 
 * The fifth rule of fight club is:
 * 	If you include ssl/ssl.h, it comes right after elinks.h.
 * 
 * The sixth rule of fight club is:
 * 	No ssl/ssl.h in other .h file, no complaints about the rules.
 * 
 * The seventh rule of fight club is:
 * 	The compilation will go as long as there is still something to compile.
 * 
 * The eight, and final rule of fight club is:
 * 	If this your first peek at this .h file, you HAVE TO COMPILE!
 * 
 * 					(Apologies to Chuck Palahniuk.) */

/* *Always* include this header file as the first non-system one after
 * elinks.h.  Screw the alphabetical order, this is a perfect example of
 * exception. We need OpenSSL's ssl.h here, but they use 'error' there in a
 * context where it actually hurts that it expands to our macro ;-). So this
 * must always come before util/error.h and you don't know what all the other
 * strange .h files actually use that one. Oh and that also implies that you
 * should never include this file in any other .h file. */

#ifdef HAVE_SSL
#ifdef HAVE_OPENSSL
#include <openssl/ssl.h>
#elif defined(HAVE_GNUTLS)
#include <gnutls/gnutls.h>
#else
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

void init_ssl(void);
void done_ssl(void);

ssl_t *get_ssl(void);
void free_ssl(ssl_t *);

unsigned char *get_ssl_cipher_str(ssl_t *);

#endif
