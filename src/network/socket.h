/* $Id: socket.h,v 1.19 2004/07/22 17:15:09 pasky Exp $ */

#ifndef EL__LOWLEVEL_CONNECT_H
#define EL__LOWLEVEL_CONNECT_H

#include <sys/types.h>
#include <sys/socket.h> /* OS/2 needs this after sys/types.h */

struct connection;

struct conn_info {
	struct sockaddr_storage *addr; /* array of addresses */

	void (*func)(struct connection *);

	int *sock;

	int addrno; /* array len / sizeof(sockaddr_storage) */
	int triedno; /* index of last tried address */
	int port;
};

struct read_buffer {
	/* A routine called *each time new data comes in*, therefore
	 * usually many times, not only when all the data arrives. */
	void (*done)(struct connection *, struct read_buffer *);

	int sock;
	int len;
	int close;
	int freespace;

	unsigned char data[1]; /* must be at end of struct */
};

void close_socket(struct connection *, int *);
void make_connection(struct connection *, int, int *, void (*)(struct connection *));
void dns_found(/* struct connection */ void *, int);
int get_pasv_socket(struct connection *, int, unsigned char *);
#ifdef CONFIG_IPV6
int get_pasv6_socket(struct connection *, int, struct sockaddr_storage *);
#endif
void write_to_socket(struct connection *, int, unsigned char *, int, void (*)(struct connection *));
struct read_buffer *alloc_read_buffer(struct connection *c);
void read_from_socket(struct connection *, int, struct read_buffer *, void (*)(struct connection *, struct read_buffer *));
void kill_buffer_data(struct read_buffer *, int);
void dns_exception(void *);

#endif
