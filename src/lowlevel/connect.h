/* $Id: connect.h,v 1.61 2005/04/13 02:17:24 jonas Exp $ */

#ifndef EL__LOWLEVEL_CONNECT_H
#define EL__LOWLEVEL_CONNECT_H

#include <sys/types.h>
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h> /* OS/2 needs this after sys/types.h */
#endif

struct connection;
struct read_buffer;
struct socket;
struct uri;


/* Use internally for error return values. */
enum socket_error {
	SOCKET_SYSCALL_ERROR	= -1,	/* Retry with -errno state. */
	SOCKET_INTERNAL_ERROR	= -2,	/* Stop with -errno state. */
	SOCKET_SSL_WANT_READ	= -3,	/* Try to read some more. */
	SOCKET_CANT_READ	= -4,	/* Retry with S_CANT_READ state. */
	SOCKET_CANT_WRITE	= -5,	/* Retry with S_CANT_WRITE state. */
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

typedef void (*socket_read_operation_T)(struct connection *, struct socket *, struct read_buffer *);
typedef void (*socket_write_operation_T)(struct connection *, struct socket *);
typedef void (*socket_connect_operation_T)(struct connection *, struct socket *);

struct read_buffer {
	/* A routine called *each time new data comes in*, therefore
	 * usually many times, not only when all the data arrives. */
	socket_read_operation_T done;

	int len;
	enum socket_state state;
	int freespace;

	unsigned char data[1]; /* must be at end of struct */
};

struct conn_info {
	struct sockaddr_storage *addr; /* array of addresses */

	socket_connect_operation_T done;

	void *dnsquery;

	int addrno; /* array len / sizeof(sockaddr_storage) */
	int triedno; /* index of last tried address */
	int port;
	int ip_family; /* If non-zero, use the indicated IP version. */
	unsigned int need_ssl:1;
};

typedef void (*socket_operation_T)(void *, struct socket *, int connection_state);

struct socket_operations {
	/* Report change in the state of the socket. */
	socket_operation_T set_state;
	/* Reset the timeout for the socket. */
	socket_operation_T set_timeout;
	/* Some system related error occured; advise to reconnect. */
	socket_operation_T retry;
	/* A fatal error occured, like a memory allocation failure; advise to
	 * abort the connection. */
	socket_operation_T done;
};

struct socket {
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
	struct socket_operations *ops;

	/* Only used by ftp in send_cmd/get_resp. Put here
	 * since having no connection->info is apparently valid. */
	socket_read_operation_T read_done;

	/* For connections using SSL this is in fact (ssl_t *), but we don't
	 * want to know. Noone cares and inclusion of SSL header files costs a
	 * lot of compilation time. --pasky */
	void *ssl;

	unsigned int protocol_family:1; /* 0 == PF_INET, 1 == PF_INET6 */
	unsigned int no_tls:1;

};

struct conn_info *
init_connection_info(struct uri *uri, struct socket *socket,
		     socket_connect_operation_T connect_done);

void done_connection_info(struct socket *socket);

void close_socket(struct socket *socket);

/* Establish connection with the host in @conn->uri. Storing the socket
 * descriptor in @socket. When the connection has been established the @done
 * callback will be run. */
void make_connection(struct connection *conn, struct socket *socket,
		     socket_connect_operation_T connect_done);

void dns_found(struct socket *, int);
void dns_exception(struct socket *);

int get_pasv_socket(struct connection *, int, unsigned char *);
#ifdef CONFIG_IPV6
int get_pasv6_socket(struct connection *, int, struct sockaddr_storage *);
#endif

/* Writes @datalen bytes from @data buffer to the passed @socket. When all data
 * is written the @done callback will be called. */
void write_to_socket(struct socket *socket,
		     unsigned char *data, int datalen,
		     int connection_state, socket_write_operation_T write_done);

struct read_buffer *alloc_read_buffer(struct socket *socket);

/* Reads data from @socket into @buffer using @done as struct read_buffers
 * @done routine (called each time new data comes in). */
void read_from_socket(struct socket *socket, struct read_buffer *buffer,
		      socket_read_operation_T done);

void kill_buffer_data(struct read_buffer *, int);

#endif
