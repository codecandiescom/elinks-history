/* $Id: ssl.h,v 1.16 2003/10/27 23:08:48 jonas Exp $ */

#ifndef EL__SSL_SSL_H
#define EL__SSL_SSL_H

#ifdef HAVE_SSL

#include "sched/connection.h"

/* Initializes the SSL connection data. Returns S_OK on success and S_SSL_ERROR
 * on failure. */
int init_ssl_connection(struct connection *conn);

/* Releases the SSL connection data */
void done_ssl_connection(struct connection *conn);

unsigned char *get_ssl_connection_cipher(struct connection *conn);

#endif /* HAVE_SSL */
#endif
