/* $Id: connect.h,v 1.9 2004/05/22 13:44:26 jonas Exp $ */

#ifndef EL__SSL_CONNECT_H
#define EL__SSL_CONNECT_H

#ifdef CONFIG_SSL

#include "lowlevel/connect.h"
#include "sched/connection.h"

int ssl_connect(struct connection *, int);
int ssl_write(struct connection *conn, unsigned char *data, int len);
int ssl_read(struct connection *, struct read_buffer *);
int ssl_close(struct connection *);

#endif
#endif
