/* SSL support - wrappers for SSL routines */
/* $Id: ssl.c,v 1.24 2003/07/06 21:25:49 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_SSL
#ifdef HAVE_OPENSSL
#include <openssl/ssl.h>
#include <openssl/rand.h>
#elif defined(HAVE_GNUTLS)
#include <gnutls/gnutls.h>
#endif
#endif

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#include "elinks.h"

/* We'll be legend. We won't grow old. */
#include "ssl/ssl.h"

#include "util/conv.h"
#include "util/error.h"
#include "util/string.h"

#ifndef PATH_MAX
#define	PATH_MAX	256 /* according to my /usr/include/bits/posix1_lim.h */
#endif


/* FIXME: As you can see, SSL is currently implemented in very, erm,
 * decentralized manner. */


#ifdef HAVE_SSL
#ifdef HAVE_OPENSSL
SSL_CTX *context = NULL;
#elif defined(HAVE_GNUTLS)
GNUTLS_ANON_CLIENT_CREDENTIALS anon_cred = NULL;
GNUTLS_CERTIFICATE_CLIENT_CREDENTIALS xcred = NULL;
#endif
#endif

void
init_ssl(void)
{
#ifdef HAVE_SSL
#ifdef HAVE_OPENSSL
	unsigned char f_randfile[PATH_MAX];

	/* In a nutshell, on OS's without a /dev/urandom, the OpenSSL library
	 * cannot initialize the PRNG and so every attempt to use SSL fails.
	 * It's actually an OpenSSL FAQ, and according to them, it's up to the
	 * application coders to seed the RNG. -- William Yodlowsky */
	if (RAND_egd(RAND_file_name(f_randfile, sizeof(f_randfile))) < 0) {
		/* Not an EGD, so read and write to it */
		if (RAND_load_file(f_randfile, -1))
			RAND_write_file(f_randfile);
	}

	SSLeay_add_ssl_algorithms();
	context = SSL_CTX_new(SSLv23_client_method());
	SSL_CTX_set_options(context, SSL_OP_ALL);
	SSL_CTX_set_default_verify_paths(context);
#elif defined(HAVE_GNUTLS)
	int ret;

	ret = gnutls_global_init();
	if (ret < 0)
		internal("GNUTLS init failed: %s", gnutls_strerror(ret));

	ret = gnutls_anon_allocate_client_sc(&anon_cred);
	if (ret < 0)
		internal("GNUTLS anon credentials alloc failed: %s",
			 gnutls_strerror(ret));

	ret = gnutls_certificate_allocate_sc(&xcred);
	if (ret < 0)
		internal("GNUTLS X509 credentials alloc failed: %s",
			 gnutls_strerror(ret));

	/* Here, we should load certificate files etc. */
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
	if (xcred) gnutls_certificate_free_sc(xcred);
	if (anon_cred) gnutls_anon_free_client_sc(anon_cred);
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
	/* XXX: GNUTLS_STATE is obviously a pointer by itself, but as it is
	 * hidden for some stupid design decision, we must not rely on that,
	 * who knows if some future implementation won't have that as a
	 * structure itself.. --pasky */
	int ret;
	GNUTLS_STATE *state = mem_alloc(sizeof(GNUTLS_STATE));

	if (!state) return NULL;

	ret = gnutls_init(state, GNUTLS_CLIENT);

	if (ret < 0) {
		/* debug("sslinit %s", gnutls_strerror(ret)); */
		mem_free(state);
		return NULL;
	}

	ret = gnutls_cred_set(*state, GNUTLS_CRD_ANON, anon_cred);
	if (ret < 0) {
		/* debug("sslanoncred %s", gnutls_strerror(ret)); */
		gnutls_deinit(*state);
		mem_free(state);
		return NULL;
	}

	ret = gnutls_cred_set(*state, GNUTLS_CRD_CERTIFICATE, xcred);
	if (ret < 0) {
		/* debug("sslx509cred %s", gnutls_strerror(ret)); */
		gnutls_deinit(*state);
		mem_free(state);
		return NULL;
	}

	return state;
#endif
#else
	return NULL;
#endif
}

void
free_ssl(ssl_t *ssl)
{
	if (!ssl) return;
#ifdef HAVE_SSL
#ifdef HAVE_OPENSSL
	SSL_free(ssl);
#elif defined(HAVE_GNUTLS)
	gnutls_deinit(*ssl);
	mem_free(ssl);
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
	add_chr_to_str(&str, &l, ' ');
	add_to_str(&str, &l, (unsigned char *) SSL_get_cipher_name(ssl));
#elif defined(HAVE_GNUTLS)
	/* XXX: How to get other relevant parameters? */
	add_to_str(&str, &l, (unsigned char *)
			gnutls_protocol_get_name(gnutls_protocol_get_version(*ssl)));
	add_to_str(&str, &l, " - ");
	add_to_str(&str, &l, (unsigned char *)
			gnutls_kx_get_name(gnutls_kx_get(*ssl)));
	add_to_str(&str, &l, " - ");
	add_to_str(&str, &l, (unsigned char *)
			gnutls_cipher_get_name(gnutls_cipher_get(*ssl)));
	add_to_str(&str, &l, " - ");
	add_to_str(&str, &l, (unsigned char *)
			gnutls_mac_get_name(gnutls_mac_get(*ssl)));
	add_to_str(&str, &l, " - ");
	add_to_str(&str, &l, (unsigned char *)
			gnutls_cert_type_get_name(gnutls_cert_type_get(*ssl)));
	add_to_str(&str, &l, " (compr:");
	add_to_str(&str, &l, (unsigned char *)
			gnutls_compression_get_name(gnutls_compression_get(*ssl)));
	add_chr_to_str(&str, &l, ')');
#endif
#endif

	return str;
}
