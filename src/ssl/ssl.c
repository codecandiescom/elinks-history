/* SSL support - wrappers for SSL routines */
/* $Id: ssl.c,v 1.10 2002/07/05 03:59:40 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_SSL
#ifdef HAVE_OPENSSL
#include <openssl/ssl.h>
#elif defined(HAVE_GNUTLS)
#include <gnutls.h>
#endif
#endif

#include "links.h"

#include "ssl/ssl.h"
#include "util/conv.h"
#include "util/string.h"


/* FIXME: As you can see, SSL is currently implemented in very, erm,
 * decentralized manner. */


#ifdef HAVE_SSL
#ifdef HAVE_OPENSSL
SSL_CTX *context = NULL;
#endif
#endif

void
init_ssl()
{
#ifdef HAVE_SSL
#ifdef HAVE_OPENSSL
	SSLeay_add_ssl_algorithms();
	context = SSL_CTX_new(SSLv23_client_method());
	SSL_CTX_set_options(context, SSL_OP_ALL);
	SSL_CTX_set_default_verify_paths(context);
#elif defined(HAVE_GNUTLS)
	gnutls_global_init();
#endif
#endif
}

void
done_ssl(void)
{
#ifdef HAVE_SSL
#ifdef HAVE_OPENSSL
	if (context) SSL_CTX_free(context);
#elif defined(HAVE_GNUTLS)
	gnutls_global_deinit();
#endif
#endif
}


ssl_t *
get_ssl(void)
{
#ifdef HAVE_SSL
#ifdef HAVE_OPENSSL
	return (SSL_new(context));
#elif defined(HAVE_GNUTLS)
	/* XXX: GNUTLS_STATE itself is obviously a pointer by itself, but as it
	 * is hidden for some stupid design decision, we must not rely on that,
	 * who knows if some future implementation won't have that as a
	 * structure itself.. --pasky */
	GNUTLS_STATE *state = mem_alloc(sizeof(GNUTLS_STATE));

	gnutls_init(state, GNUTLS_CLIENT);
	return state;
#endif
#else
	return NULL;
#endif
}

void
free_ssl(ssl_t *ssl)
{
#ifdef HAVE_SSL
#ifdef HAVE_OPENSSL
	SSL_free(ssl);
#elif defined(HAVE_GNUTLS)
	gnutls_deinit(*ssl);
#endif
#endif
}


unsigned char *
get_ssl_cipher_str(ssl_t *ssl) {
	unsigned char *str = NULL;
#ifdef HAVE_SSL
	int l = 0;

	str = init_str();
	if (!str) return NULL;

#ifdef HAVE_OPENSSL
	add_num_to_str(&str, &l, SSL_get_cipher_bits(ssl, NULL));
	add_to_str(&str, &l, "-bit ");
	add_to_str(&str, &l, SSL_get_cipher_version(ssl));
	add_to_str(&str, &l, " ");
	add_to_str(&str, &l, (unsigned char *) SSL_get_cipher_name(ssl));
#elif defined(HAVE_GNUTLS)
	/* XXX: How to get other relevant parameters? */
	add_to_str(&str, &l, (unsigned char *)
			gnutls_protocol_get_name(gnutls_protocl_get_version(*ssl)));
	add_to_str(&str, &l, " ");
	add_to_str(&str, &l, (unsigned char *)
			gnutls_cipher_get_name(gnutls_cipher_get(*ssl)));
	add_to_str(&str, &l, " ");
	add_to_str(&str, &l, (unsigned char *)
			gnutls_mac_get_name(gnutls_mac_get(*ssl)));
#endif
#endif

	return str;
}
