/* $Id: connect.h,v 1.1 2002/03/17 23:16:52 pasky Exp $ */

#ifndef EL__SSL_CONNECT_H
#define EL__SSL_CONNECT_H

#ifdef HAVE_SSL

#include <lowlevel/connect.h>
#include <lowlevel/sched.h>

int ssl_connect(struct connection *, int);
int ssl_write(struct connection *, struct write_buffer *);
int ssl_read(struct connection *, struct read_buffer *);

#endif

#endif
