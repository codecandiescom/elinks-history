/* $Id: connect.h,v 1.7 2003/10/27 22:32:33 jonas Exp $ */

#ifndef EL__SSL_CONNECT_H
#define EL__SSL_CONNECT_H

#ifdef HAVE_SSL

#include "lowlevel/connect.h"
#include "sched/connection.h"

int ssl_connect(struct connection *, int);
int ssl_write(struct connection *, struct write_buffer *);
int ssl_read(struct connection *, struct read_buffer *);
int ssl_close(struct connection *);

#endif
#endif
