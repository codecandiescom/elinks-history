/* $Id: socket.h,v 1.4 2002/07/05 03:59:40 pasky Exp $ */

#ifndef EL__SSL_CONNECT_H
#define EL__SSL_CONNECT_H

#include "lowlevel/connect.h"
#include "lowlevel/sched.h"

int ssl_connect(struct connection *, int);
int ssl_write(struct connection *, struct write_buffer *);
int ssl_read(struct connection *, struct read_buffer *);
int ssl_close(struct connection *);

#endif
