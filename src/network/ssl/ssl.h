/* $Id: ssl.h,v 1.19 2004/04/29 23:22:15 jonas Exp $ */

#ifndef EL__SSL_SSL_H
#define EL__SSL_SSL_H

#ifdef CONFIG_SSL

#include "modules/module.h"
#include "sched/connection.h"

extern struct module ssl_module;

/* Initializes the SSL connection data. Returns S_OK on success and S_SSL_ERROR
 * on failure. */
int init_ssl_connection(struct connection *conn);

/* Releases the SSL connection data */
void done_ssl_connection(struct connection *conn);

unsigned char *get_ssl_connection_cipher(struct connection *conn);


/* Internal type used in ssl module. */

#ifdef CONFIG_OPENSSL
#define	ssl_t	SSL
#elif defined(CONFIG_GNUTLS)
#define	ssl_t	GNUTLS_STATE
#endif

#endif /* CONFIG_SSL */
#endif
