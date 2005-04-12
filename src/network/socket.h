/* $Id: socket.h,v 1.50 2005/04/12 16:53:33 jonas Exp $ */

#ifndef EL__LOWLEVEL_CONNECT_H
#define EL__LOWLEVEL_CONNECT_H

#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h> /* OS/2 needs this after sys/types.h */
#endif

struct connection;
struct uri;

struct conn_info {
	struct sockaddr_storage *addr; /* array of addresses */

	void (*done)(struct connection *);

	void *dnsquery;

	int addrno; /* array len / sizeof(sockaddr_storage) */
	int triedno; /* index of last tried address */
	int port;
	int ip_family; /* If non-zero, use the indicated IP version. */
	unsigned int need_ssl:1;
};

enum socket_state {
	/* If a zero-byte message is read prematurely the connection will be
	 * retried with error state S_CANT_READ. */
	SOCKET_RETRY_ONCLOSE,
	/* If a zero-byte message is read flush the remaining bytes in the
	 * buffer and tell the protocol handler to end the reading by calling
	 * read_buffer->done(). */
	SOCKET_END_ONCLOSE,
	/* Used for signaling to protocols - via the read_buffer->done()
	 * callback - that a zero-byte message was read. */
	SOCKET_CLOSED,
};

struct read_buffer {
	/* A routine called *each time new data comes in*, therefore
	 * usually many times, not only when all the data arrives. */
	void (*done)(struct connection *, struct read_buffer *);

	int len;
	enum socket_state state;
	int freespace;

	unsigned char data[1]; /* must be at end of struct */
};

enum socket_error {
	SOCKET_SYSCALL_ERROR	= -1,	/* Retry with -errno state. */
	SOCKET_INTERNAL_ERROR	= -2,	/* Stop with -errno state. */
	SOCKET_WANT_READ	= -3,	/* Try to read some more. */
	SOCKET_CANT_READ	= -4,	/* Retry with S_CANT_READ state. */
	SOCKET_CANT_WRITE	= -5,	/* Retry with S_CANT_WRITE state. */
};

typedef void (*connection_socket_handler_T)(void *, int connection_state);

struct connection_socket {
	/* The socket descriptor */
	int fd;

	/* Information for resolving the connection with which the socket is
	 * associated. */
	void *conn;

	/* Information used during the connection establishing phase. */
	struct conn_info *conn_info;

	/* Use for read and write buffers. */
	void *buffer;

	/* Callbacks to the connection management: */

	/* Report change in the state of the socket. */
	connection_socket_handler_T set_state;
	/* Reset the timeout for the socket. */
	void (*set_timeout)(void *);
	/* Some system related error occured; advise to reconnect. */
	connection_socket_handler_T retry;
	/* A fatal error occured, like a memory allocation failure; advise to
	 * abort the connection. */
	connection_socket_handler_T done;
	/* Only used by ftp in send_cmd/get_resp. Put here
	 * since having no connection->info is apparently valid. */
	void (*read_done)(void *, struct read_buffer *);

	/* For connections using SSL this is in fact (ssl_t *), but we don't
	 * want to know. Noone cares and inclusion of SSL header files costs a
	 * lot of compilation time. --pasky */
	void *ssl;

	unsigned int protocol_family:1; /* 0 == PF_INET, 1 == PF_INET6 */
	unsigned int no_tls:1;

};

struct conn_info *
init_connection_info(struct uri *uri, struct connection_socket *socket,
		     void (*done)(struct connection *));

void done_connection_info(struct connection_socket *socket);


void close_socket(struct connection_socket *socket);

/* Establish connection with the host in @conn->uri. Storing the socket
 * descriptor in @socket. When the connection has been established the @done
 * callback will be run. */
void make_connection(struct connection *conn, struct connection_socket *socket,
		     void (*done)(struct connection *));

void dns_found(struct connection_socket *, int);
void dns_exception(void *);

int get_pasv_socket(struct connection *, int, unsigned char *);
#ifdef CONFIG_IPV6
int get_pasv6_socket(struct connection *, int, struct sockaddr_storage *);
#endif

/* Writes @datalen bytes from @data buffer to the passed @socket. When all data
 * is written the @done callback will be called. */
void write_to_socket(struct connection_socket *socket,
		     unsigned char *data, int datalen, void (*done)(struct connection *));

struct read_buffer *alloc_read_buffer(struct connection_socket *socket);

/* Reads data from @socket into @buffer using @done as struct read_buffers
 * @done routine (called each time new data comes in). */
void read_from_socket(struct connection_socket *socket, 
		       struct read_buffer *buffer, void (*done)(struct connection *, struct read_buffer *));

void kill_buffer_data(struct read_buffer *, int);

#endif
