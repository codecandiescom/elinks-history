/* $Id: ssl.h,v 1.25 2005/06/13 00:43:28 jonas Exp $ */

#ifndef EL__NETWORK_SSL_SSL_H
#define EL__NETWORK_SSL_SSL_H

#ifdef CONFIG_SSL

#include "main/module.h"

struct socket;

extern struct module ssl_module;

/* Initializes the SSL connection data. Returns S_OK on success and S_SSL_ERROR
 * on failure. */
int init_ssl_connection(struct socket *socket);

/* Releases the SSL connection data */
void done_ssl_connection(struct socket *socket);

unsigned char *get_ssl_connection_cipher(struct socket *socket);


/* Internal type used in ssl module. */

#ifdef CONFIG_OPENSSL
#define	ssl_t	SSL
#elif defined(CONFIG_GNUTLS)
#define	ssl_t	GNUTLS_STATE
#endif

#endif /* CONFIG_SSL */
#endif
