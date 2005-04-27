/* $Id: socket.h,v 1.21 2005/04/27 15:15:01 jonas Exp $ */

#ifndef EL__SSL_CONNECT_H
#define EL__SSL_CONNECT_H

#ifdef CONFIG_SSL

struct socket;

int ssl_connect(struct socket *socket);
ssize_t ssl_write(struct socket *socket, unsigned char *data, int len);
ssize_t ssl_read(struct socket *socket, unsigned char *data, int len);
int ssl_close(struct socket *socket);

#endif
#endif
