/* SSL support - wrappers for SSL routines */
/* $Id: ssl.c,v 1.2 2002/05/08 13:55:06 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_SSL
#include <openssl/ssl.h>
#endif

#include "links.h"

/* #include "lowlevel/sched.h" */
#include "ssl/ssl.h"

/* FIXME: As you can see, SSL is currently implemented in very, erm,
 * decentralized manner. */

#ifdef HAVE_SSL

SSL_CTX *context = 0;

SSL *getSSL(void)
{
	if (!context) {
		SSLeay_add_ssl_algorithms();
		context = SSL_CTX_new(SSLv23_client_method());
		SSL_CTX_set_options(context, SSL_OP_ALL);
		SSL_CTX_set_default_verify_paths(context);
	}
	return (SSL_new(context));
}
void ssl_finish(void)
{
	if (context) SSL_CTX_free(context);
}

#endif
