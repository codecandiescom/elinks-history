/* $Id: connect.h,v 1.6 2003/07/03 01:03:36 jonas Exp $ */

#ifndef EL__SSL_CONNECT_H
#define EL__SSL_CONNECT_H

#include "lowlevel/connect.h"
#include "sched/connection.h"

int ssl_connect(struct connection *, int);
int ssl_write(struct connection *, struct write_buffer *);
int ssl_read(struct connection *, struct read_buffer *);
int ssl_close(struct connection *);

#endif
