/* $Id: socket.h,v 1.28 2004/08/03 10:31:16 jonas Exp $ */

#ifndef EL__LOWLEVEL_CONNECT_H
#define EL__LOWLEVEL_CONNECT_H

#include <sys/types.h>
#include <sys/socket.h> /* OS/2 needs this after sys/types.h */

struct connection;
struct connection_socket;
struct uri;

struct conn_info {
	struct sockaddr_storage *addr; /* array of addresses */

	void (*done)(struct connection *);

	struct connection_socket *socket;

	int addrno; /* array len / sizeof(sockaddr_storage) */
	int triedno; /* index of last tried address */
	int port;
};

struct read_buffer {
	/* A routine called *each time new data comes in*, therefore
	 * usually many times, not only when all the data arrives. */
	void (*done)(struct connection *, struct read_buffer *);

	struct connection_socket *socket;
	int len;
	int close;
	int freespace;

	unsigned char data[1]; /* must be at end of struct */
};


struct conn_info *
init_connection_info(struct uri *uri, struct connection_socket *socket,
		     void (*done)(struct connection *));

void done_connection_info(struct connection *conn);


void close_socket(struct connection *conn, struct connection_socket *socket);

/* Establish connection with the host in @conn->uri. Storing the socket
 * descriptor in @socket. When the connection has been established the @done
 * callback will be run. */
void make_connection(struct connection *conn, struct connection_socket *socket,
		     void (*done)(struct connection *));

void dns_found(/* struct connection */ void *, int);
int get_pasv_socket(struct connection *, int, unsigned char *);
#ifdef CONFIG_IPV6
int get_pasv6_socket(struct connection *, int, struct sockaddr_storage *);
#endif

/* Writes @datalen bytes from @data buffer to the passed @socket. When all data
 * is written the @done callback will be called. */
void write_to_socket(struct connection *conn, struct connection_socket *socket,
		     unsigned char *data, int datalen, void (*done)(struct connection *));

struct read_buffer *alloc_read_buffer(struct connection *c);

/* Reads data from @socket into @buffer using @done as struct read_buffers
 * @done routine (called each time new data comes in). */
void read_from_socket(struct connection *conn, struct connection_socket *socket,
		      struct read_buffer *buffer, void (*done)(struct connection *, struct read_buffer *));

void kill_buffer_data(struct read_buffer *, int);
void dns_exception(void *);

#endif
