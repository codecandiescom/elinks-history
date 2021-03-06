/* $Id: socket.h,v 1.22 2005/06/12 02:26:49 jonas Exp $ */

#ifndef EL__NETWORK_SSL_SOCKET_H
#define EL__NETWORK_SSL_SOCKET_H

#ifdef CONFIG_SSL

struct socket;

int ssl_connect(struct socket *socket);
ssize_t ssl_write(struct socket *socket, unsigned char *data, int len);
ssize_t ssl_read(struct socket *socket, unsigned char *data, int len);
int ssl_close(struct socket *socket);

#endif
#endif
