/* $Id: socket.h,v 1.2 2002/05/08 13:55:06 pasky Exp $ */

#ifndef EL__SSL_CONNECT_H
#define EL__SSL_CONNECT_H

#ifdef HAVE_SSL

#include "lowlevel/connect.h"
#include "lowlevel/sched.h"

int ssl_connect(struct connection *, int);
int ssl_write(struct connection *, struct write_buffer *);
int ssl_read(struct connection *, struct read_buffer *);

#endif

#endif
