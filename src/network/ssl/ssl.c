/* SSL support - wrappers for SSL routines */
/* $Id: ssl.c,v 1.6 2002/07/05 01:54:25 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_SSL
#include <openssl/ssl.h>
#endif

#include "links.h"

#include "ssl/ssl.h"


/* FIXME: As you can see, SSL is currently implemented in very, erm,
 * decentralized manner. */

#ifdef HAVE_SSL
SSL_CTX *context = NULL;
#endif

void
init_ssl()
{
#ifdef HAVE_SSL
	SSLeay_add_ssl_algorithms();
	context = SSL_CTX_new(SSLv23_client_method());
	SSL_CTX_set_options(context, SSL_OP_ALL);
	SSL_CTX_set_default_verify_paths(context);
#endif
}

ssl_t *
get_ssl(void)
{
#ifdef HAVE_SSL
	return (SSL_new(context));
#else
	return NULL;
#endif
}

void
free_ssl(ssl_t *ssl)
{
#ifdef HAVE_SSL
	SSL_free(ssl);
#endif
}


void
done_ssl(void)
{
#ifdef HAVE_SSL
	if (context) SSL_CTX_free(context);
#endif
}
