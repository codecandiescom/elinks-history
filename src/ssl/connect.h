/* $Id: connect.h,v 1.5 2003/01/01 20:30:36 pasky Exp $ */

#ifndef EL__SSL_CONNECT_H
#define EL__SSL_CONNECT_H

#include "lowlevel/connect.h"
#include "sched/sched.h"

int ssl_connect(struct connection *, int);
int ssl_write(struct connection *, struct write_buffer *);
int ssl_read(struct connection *, struct read_buffer *);
int ssl_close(struct connection *);

#endif
