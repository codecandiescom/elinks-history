/* SSL support - wrappers for SSL routines */
/* $Id: ssl.c,v 1.37 2003/10/27 23:43:58 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_SSL

#ifdef HAVE_OPENSSL
#include <openssl/ssl.h>
#include <openssl/rand.h>
#elif defined(HAVE_GNUTLS)
#include <gnutls/gnutls.h>
#else
#error "Huh?! You have SSL enabled, but not OPENSSL nor GNUTLS!! And then you want exactly *what* from me?"
#endif

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif

#include "elinks.h"

#include "intl/gettext/libintl.h"
#include "modules/module.h"
#include "sched/connection.h"
#include "ssl/ssl.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/string.h"


#ifndef PATH_MAX
#define	PATH_MAX	256 /* according to my /usr/include/bits/posix1_lim.h */
#endif


/* FIXME: As you can see, SSL is currently implemented in very, erm,
 * decentralized manner. */


#ifdef HAVE_OPENSSL

SSL_CTX *context = NULL;

static void
init_openssl(struct module *module)
{
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
}

static void
done_openssl(struct module *module)
{
	if (context) SSL_CTX_free(context);
}

static struct module openssl_module = struct_module(
	/* name: */		"OpenSSL",
	/* options: */		NULL,
	/* events: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		init_openssl,
	/* done: */		done_openssl
);

#elif defined(HAVE_GNUTLS)

GNUTLS_ANON_CLIENT_CREDENTIALS anon_cred = NULL;
GNUTLS_CERTIFICATE_CLIENT_CREDENTIALS xcred = NULL;

const static int protocol_priority[16] = {
	GNUTLS_TLS1, GNUTLS_SSL3, 0
};
const static int kx_priority[16] = {
	GNUTLS_KX_RSA, GNUTLS_KX_DHE_DSS, GNUTLS_KX_DHE_RSA, GNUTLS_KX_SRP,
	/* Do not use anonymous authentication, unless you know what that means */
	GNUTLS_KX_ANON_DH, GNUTLS_KX_RSA_EXPORT, 0
};
const static int cipher_priority[16] = {
	GNUTLS_CIPHER_ARCFOUR_128, GNUTLS_CIPHER_RIJNDAEL_128_CBC,
	GNUTLS_CIPHER_3DES_CBC, GNUTLS_CIPHER_ARCFOUR_40, 0
};
const static int comp_priority[16] = { GNUTLS_COMP_ZLIB, GNUTLS_COMP_NULL, 0 };
const static int mac_priority[16] = { GNUTLS_MAC_SHA, GNUTLS_MAC_MD5, 0 };
const static int cert_type_priority[16] = { GNUTLS_CRT_X509, GNUTLS_CRT_OPENPGP, 0 };

static void
init_gnutls(struct module *module)
{
	if (gnutls_global_init() < 0)
		internal("GNUTLS init failed: %s", gnutls_strerror(ret));

	if (gnutls_anon_allocate_client_sc(&anon_cred) < 0)
		internal("GNUTLS anon credentials alloc failed: %s",
			 gnutls_strerror(ret));

	if (gnutls_certificate_allocate_sc(&xcred) < 0)
		internal("GNUTLS X509 credentials alloc failed: %s",
			 gnutls_strerror(ret));

	/* Here, we should load certificate files etc. */
}

static void
done_gnutls(struct module *module)
{
	if (xcred) gnutls_certificate_free_sc(xcred);
	if (anon_cred) gnutls_anon_free_client_sc(anon_cred);
	gnutls_global_deinit();
}

static struct module gnutls_module = struct_module(
	/* name: */		"GnuTLS",
	/* options: */		NULL,
	/* events: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		init_gnutls,
	/* done: */		done_gnutls
);

#endif /* HAVE_OPENSSL or HAVE_GNUTLS */

static struct module *ssl_modules[] = {
#ifdef HAVE_OPENSSL
	&openssl_module,
#elif defined(HAVE_GNUTLS)
	&gnutls_module,
#endif
	NULL,
};

struct module ssl_module = struct_module(
	/* name: */		N_("SSL"),
	/* options: */		NULL,
	/* events: */		NULL,
	/* submodules: */	ssl_modules,
	/* data: */		NULL,
	/* init: */		NULL,
	/* done: */		NULL
);

int
init_ssl_connection(struct connection *conn)
{
#ifdef HAVE_OPENSSL
	conn->ssl = SSL_new(context);
	if (!conn->ssl) return S_SSL_ERROR;
#elif defined(HAVE_GNUTLS)
	ssl_t *state = mem_alloc(sizeof(GNUTLS_STATE));

	if (!state) return S_SSL_ERROR;

	if (gnutls_init(state, GNUTLS_CLIENT) < 0) {
		/* debug("sslinit %s", gnutls_strerror(ret)); */
		mem_free(state);
		return S_SSL_ERROR;
	}

	if (gnutls_cred_set(*state, GNUTLS_CRD_ANON, anon_cred) < 0) {
		/* debug("sslanoncred %s", gnutls_strerror(ret)); */
		gnutls_deinit(*state);
		mem_free(state);
		return S_SSL_ERROR;
	}

	if (gnutls_cred_set(*state, GNUTLS_CRD_CERTIFICATE, xcred) < 0) {
		/* debug("sslx509cred %s", gnutls_strerror(ret)); */
		gnutls_deinit(*state);
		mem_free(state);
		return S_SSL_ERROR;
	}

	gnutls_handshake_set_private_extensions(*state, 1);
	gnutls_cipher_set_priority(*state, cipher_priority);
	gnutls_compression_set_priority(*state, comp_priority);
	gnutls_kx_set_priority(*state, kx_priority);
	gnutls_protocol_set_priority(*state, protocol_priority);
	gnutls_mac_set_priority(*state, mac_priority);
	gnutls_certificate_type_set_priority(*state, cert_type_priority);
	gnutls_set_server_name(*state, GNUTLS_NAME_DNS, "localhost", strlen("localhost"));

	conn->ssl = state;
#endif

	return S_OK;
}

void
done_ssl_connection(struct connection *conn)
{
	ssl_t *ssl = conn->ssl;

	if (!ssl) return;
#ifdef HAVE_OPENSSL
	SSL_free(ssl);
#elif defined(HAVE_GNUTLS)
	gnutls_deinit(*ssl);
	mem_free(ssl);
#endif
	conn->ssl = NULL;
}

unsigned char *
get_ssl_connection_cipher(struct connection *conn)
{
	ssl_t *ssl = conn->ssl;
	struct string str = NULL_STRING;

	if (!init_string(&str)) return NULL;

#ifdef HAVE_OPENSSL
	add_long_to_string(&str, SSL_get_cipher_bits(ssl, NULL));
	add_to_string(&str, "-bit ");
	add_to_string(&str, SSL_get_cipher_version(ssl));
	add_char_to_string(&str, ' ');
	add_to_string(&str, (unsigned char *) SSL_get_cipher_name(ssl));
#elif defined(HAVE_GNUTLS)
	/* XXX: How to get other relevant parameters? */
	add_to_string(&str, (unsigned char *)
		      gnutls_protocol_get_name(gnutls_protocol_get_version(*ssl)));
	add_to_string(&str, " - ");
	add_to_string(&str, (unsigned char *)
		      gnutls_kx_get_name(gnutls_kx_get(*ssl)));
	add_to_string(&str, " - ");
	add_to_string(&str, (unsigned char *)
			gnutls_cipher_get_name(gnutls_cipher_get(*ssl)));
	add_to_string(&str, " - ");
	add_to_string(&str, (unsigned char *)
			gnutls_mac_get_name(gnutls_mac_get(*ssl)));
	add_to_string(&str, " - ");
	add_to_string(&str, (unsigned char *)
			gnutls_cert_type_get_name(gnutls_cert_type_get(*ssl)));
	add_to_string(&str, " (compr:");
	add_to_string(&str, (unsigned char *)
			gnutls_compression_get_name(gnutls_compression_get(*ssl)));
	add_char_to_string(&str, ')');
#endif

	return str.source;
}

#endif /* HAVE_SSL */
