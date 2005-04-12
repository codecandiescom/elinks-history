/* $Id: connect.h,v 1.20 2005/04/12 21:49:09 jonas Exp $ */

#ifndef EL__SSL_CONNECT_H
#define EL__SSL_CONNECT_H

#ifdef CONFIG_SSL

struct socket;

int ssl_connect(struct socket *socket);
int ssl_write(struct socket *socket, unsigned char *data, int len);
int ssl_read(struct socket *socket, unsigned char *data, int len);
int ssl_close(struct socket *socket);

#endif
#endif
