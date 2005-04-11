/* $Id: socket.h,v 1.17 2005/04/11 22:50:34 jonas Exp $ */

#ifndef EL__SSL_CONNECT_H
#define EL__SSL_CONNECT_H

#ifdef CONFIG_SSL

#include "lowlevel/connect.h"
#include "sched/connection.h"

int ssl_connect(struct connection_socket *socket);
int ssl_write(struct connection_socket *socket, unsigned char *data, int len);
int ssl_read(struct connection_socket *socket, struct read_buffer *);
int ssl_close(struct connection_socket *socket);

#endif
#endif
