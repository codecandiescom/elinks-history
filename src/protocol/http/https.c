/* Internal "https" protocol implementation */
/* $Id: https.c,v 1.2 2002/03/17 17:42:58 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_SSL
#include <openssl/ssl.h>
#endif

#include <links.h>

#include <lowlevel/sched.h>
#include <protocol/http/http.h>
#include <protocol/http/https.h>

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

void https_func(struct connection *c)
{
	c->ssl = (void *)-1;
	http_func(c);
}

#else

void https_func(struct connection *c)
{
	setcstate(c, S_NO_SSL);
	abort_connection(c);
}

#endif
