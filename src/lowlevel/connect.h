/* $Id: connect.h,v 1.7 2002/05/08 13:55:04 pasky Exp $ */

#ifndef EL__LOWLEVEL_CONNECT_H
#define EL__LOWLEVEL_CONNECT_H

/* We MAY have problems with this. If there will be any, just tell me, and
 * I will move it to start of links.h. */
#include <sys/types.h>
#include <sys/socket.h> /* OS/2 needs this after sys/types.h */

#include "lowlevel/sched.h"

#define READ_SIZE 16384

struct conn_info {
	struct sockaddr *addr; /* array of addresses */
	int addrno; /* array len / sizeof(sockaddr) */
	int triedno; /* index of last tried address */
	int port;
	int *sock;
	void (*func)(struct connection *);
};

struct read_buffer {
	int sock;
	int len;
	int close;
	void (*done)(struct connection *, struct read_buffer *);
	unsigned char data[1];
};

struct write_buffer {
	int sock;
	int len;
	int pos;
	void (*done)(struct connection *);
	unsigned char data[1];
};

void close_socket(int *);
void make_connection(struct connection *, int, int *, void (*)(struct connection *));
void dns_found(/* struct connection */ void *, int);
int get_pasv_socket(struct connection *, int, unsigned char *);
void write_to_socket(struct connection *, int, unsigned char *, int, void (*)(struct connection *));
struct read_buffer *alloc_read_buffer(struct connection *c);
void read_from_socket(struct connection *, int, struct read_buffer *, void (*)(struct connection *, struct read_buffer *));
void kill_buffer_data(struct read_buffer *, int);
void dns_exception(void *);

#endif
