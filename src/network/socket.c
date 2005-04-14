/* Sockets-o-matic */
/* $Id: socket.c,v 1.188 2005/04/14 00:40:55 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <string.h>
#include <sys/types.h>
#ifdef HAVE_NETINET_IN_H
#include <netinet/in.h> /* OS/2 needs this after sys/types.h */
#endif
#ifdef HAVE_SYS_SOCKET_H
#include <sys/socket.h> /* OS/2 needs this after sys/types.h */
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h> /* OS/2 needs this after sys/types.h */
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_GETIFADDRS
#ifdef HAVE_NETDB_H
#include <netdb.h>
#endif
#ifdef HAVE_NET_IF_H
#include <net/if.h>
#endif
#ifdef HAVE_IFADDRS_H
#include <ifaddrs.h>		/* getifaddrs() */
#endif
#endif				/* HAVE_GETIFADDRS */

#include "elinks.h"

#include "config/options.h"
#include "lowlevel/connect.h"
#include "lowlevel/dns.h"
#include "lowlevel/select.h"
#include "osdep/osdep.h"
#include "osdep/getifaddrs.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "sched/connection.h"
#include "ssl/connect.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"

/* To enable logging of tranfers, for debugging purposes. */
#if 0

#define DEBUG_TRANSFER_LOGFILE "/tmp/log"

static void
debug_transfer_log(unsigned char *data, int len)
{
	int fd = open(DEBUG_TRANSFER_LOGFILE, O_WRONLY | O_APPEND | O_CREAT, 0622);

	if (fd == -1) return;

	set_bin(fd);
	write(fd, data, len < 0 ? strlen(data) : len);
	close(fd);
}
#undef DEBUG_TRANSFER_LOGFILE

#else
#define debug_transfer_log(data, len)
#endif


static void connected(struct socket *socket);

struct socket *
init_socket(void *conn, struct socket_operations *ops)
{
	struct socket *socket;

	socket = mem_calloc(1, sizeof(*socket));
	if (!socket) return NULL;

	socket->fd = -1;
	socket->conn = conn;
	socket->ops = ops;

	return socket;
}

void
close_socket(struct socket *socket)
{
	if (socket->fd == -1) return;
#ifdef CONFIG_SSL
	if (socket->ssl) ssl_close(socket);
#endif
	close(socket->fd);
	clear_handlers(socket->fd);
	socket->fd = -1;
}

void
dns_exception(struct socket *socket)
{
	socket->ops->set_state(socket->conn, socket, S_EXCEPT);
	close_socket(socket);
	connect_socket(socket);
}

static void
exception(struct socket *socket)
{
	socket->ops->retry(socket->conn, socket, S_EXCEPT);
}


struct conn_info *
init_connection_info(struct uri *uri, struct socket *socket,
		     socket_connect_operation_T connect_done)
{
	struct conn_info *conn_info = mem_calloc(1, sizeof(*conn_info));

	if (!conn_info) return NULL;

	conn_info->done = connect_done;
	conn_info->port = get_uri_port(uri);
	conn_info->ip_family = uri->ip_family;
	conn_info->need_ssl = get_protocol_need_ssl(uri->protocol);
	conn_info->triedno = -1;
	conn_info->addr = NULL;

	return conn_info;
}

void
done_connection_info(struct socket *socket)
{
	struct conn_info *conn_info = socket->conn_info;

	assert(socket->conn_info);

	if (conn_info->dnsquery) kill_dns_request(&conn_info->dnsquery);
	if (conn_info->done) conn_info->done(socket->conn, socket);

	mem_free_if(conn_info->addr);
	mem_free_set(&socket->conn_info, NULL);
}

/* DNS callback. */
static void
dns_found(struct socket *socket, struct sockaddr_storage *addr, int addrlen)
{
	if (!addr) {
		socket->ops->done(socket->conn, socket, S_NO_DNS);
		return;
	}

	assert(socket->conn_info);

	socket->conn_info->addr	  = addr;
	socket->conn_info->addrno = addrlen;

	connect_socket(socket);
}

void
make_connection(struct connection *conn, struct socket *socket,
		socket_connect_operation_T connect_done)
{
	unsigned char *host = get_uri_string(conn->uri, URI_DNS_HOST);
	struct conn_info *conn_info;
	int async;

	socket->ops->set_timeout(socket->conn, socket, 0);

	if (!host) {
		socket->ops->retry(socket->conn, socket, S_OUT_OF_MEM);
		return;
	}

	conn_info = init_connection_info(conn->uri, socket, connect_done);
	if (!conn_info) {
		mem_free(host);
		socket->ops->retry(socket->conn, socket, S_OUT_OF_MEM);
		return;
	}

	socket->conn_info = conn_info;

	debug_transfer_log("\nCONNECTION: ", -1);
	debug_transfer_log(host, -1);
	debug_transfer_log("\n", -1);

	async = find_host(host, &conn_info->dnsquery, (dns_callback_T) dns_found,
			  socket, conn->cache_mode >= CACHE_MODE_FORCE_RELOAD);

	mem_free(host);

	if (async) socket->ops->set_state(socket->conn, socket, S_DNS);
}


/* Returns negative if error, otherwise pasv socket's fd. */
int
get_pasv_socket(struct connection *conn, int ctrl_sock, unsigned char *port)
{
	struct sockaddr_in sa, sb;
	int sock, len;

	memset(&sa, 0, sizeof(sa));
	memset(&sb, 0, sizeof(sb));

	/* Get our endpoint of the control socket */
	len = sizeof(sa);
	if (getsockname(ctrl_sock, (struct sockaddr *) &sa, &len)) {
sock_error:
		retry_connection(conn, -errno);
		return -1;
	}

	/* Get a passive socket */

	sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock < 0)
		goto sock_error;

	/* Set it non-blocking */

	if (set_nonblocking_fd(sock) < 0)
		goto sock_error;

	/* Bind it to some port */

	copy_struct(&sb, &sa);
	sb.sin_port = 0;
	if (bind(sock, (struct sockaddr *) &sb, sizeof(sb)))
		goto sock_error;

	/* Get our endpoint of the passive socket and save it to port */

	len = sizeof(sa);
	if (getsockname(sock, (struct sockaddr *) &sa, &len))
		goto sock_error;
	memcpy(port, &sa.sin_addr.s_addr, 4);
	memcpy(port + 4, &sa.sin_port, 2);

	/* Go listen */

	if (listen(sock, 1))
		goto sock_error;

	set_ip_tos_throughput(sock);

	return sock;
}

#ifdef CONFIG_IPV6
int
get_pasv6_socket(struct connection *conn, int ctrl_sock,
		 struct sockaddr_storage *s6)
{
	int sock;
	struct sockaddr_in6 s0;
	int len = sizeof(s0);

	memset(&s0, 0, sizeof(s0));
	memset(s6, 0, sizeof(*s6));

	/* Get our endpoint of the control socket */

	if (getsockname(ctrl_sock, (struct sockaddr *) s6, &len)) {
sock_error:
		retry_connection(conn, -errno);
		return -1;
	}

	/* Get a passive socket */

	sock = socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP);
	if (sock < 0)
		goto sock_error;

	/* Set it non-blocking */

	if (set_nonblocking_fd(sock) < 0)
		goto sock_error;

	/* Bind it to some port */

	memcpy(&s0, s6, sizeof(s0));
	s0.sin6_port = 0;
	if (bind(sock, (struct sockaddr *) &s0, sizeof(s0)))
		goto sock_error;

	/* Get our endpoint of the passive socket and save it to port */

	len = sizeof(s0);
	if (getsockname(sock, (struct sockaddr *) s6, &len))
		goto sock_error;

	/* Go listen */

	if (listen(sock, 1))
		goto sock_error;

	set_ip_tos_throughput(sock);

	return sock;
}

static inline int
check_if_local_address6(struct sockaddr_in6 *addr)
{
	struct ifaddrs *ifaddrs;
	int local = IN6_IS_ADDR_LOOPBACK(&(addr->sin6_addr));

	if (!local && !getifaddrs(&ifaddrs)) {
		struct ifaddrs *ifa;

		for (ifa = ifaddrs; ifa; ifa = ifa->ifa_next) {
			if (!ifa->ifa_addr)
				continue;

			if (ifa->ifa_addr->sa_family == AF_INET6
			    && !memcmp(&addr->sin6_addr.s6_addr,
			    &((struct sockaddr_in6 *) ifa->ifa_addr)->sin6_addr.s6_addr,
			    sizeof(addr->sin6_addr.s6_addr))) {
				local = 1;
				break;
			}

			if (ifa->ifa_addr->sa_family == AF_INET
			    && !memcmp(&((struct sockaddr_in *) &addr)->sin_addr.s_addr,
				&((struct sockaddr_in *) ifa->ifa_addr)->sin_addr.s_addr,
				sizeof(((struct sockaddr_in *) &addr)->sin_addr.s_addr))) {
					local = 1;
					break;
			}
		}

		freeifaddrs(ifaddrs);
	}

	return local;
}
#endif /* CONFIG_IPV6 */

static inline int
check_if_local_address4(struct sockaddr_in *addr)
{
	struct ifaddrs *ifaddrs;
	int local = (ntohl(addr->sin_addr.s_addr) >> 24) == IN_LOOPBACKNET;

	if (!local && !getifaddrs(&ifaddrs)) {
		struct ifaddrs *ifa;

		for (ifa = ifaddrs; ifa; ifa = ifa->ifa_next) {
			if (!ifa->ifa_addr)
				continue;

			if (ifa->ifa_addr->sa_family != AF_INET) continue;

			if (!memcmp(&addr->sin_addr.s_addr,
				&((struct sockaddr_in *) ifa->ifa_addr)->sin_addr.s_addr,
				sizeof(addr->sin_addr.s_addr))) {
					local = 1;
					break;
			}
		}

		freeifaddrs(ifaddrs);
	}

	return local;
}


void
connect_socket(struct socket *conn_socket)
{
	int sock = -1;
	struct conn_info *conn_info = conn_socket->conn_info;
	int i;
	int trno = conn_info->triedno;
	int only_local = get_cmd_opt_bool("localhost");
	int saved_errno = 0;
	int at_least_one_remote_ip = 0;
	/* We tried something but we failed in such a way that we would rather
	 * prefer the connection to retain the information about previous
	 * failures.  That is, we i.e. decided we are forbidden to even think
	 * about such a connection attempt.
	 * XXX: Unify with @local_only handling? --pasky */
	int silent_fail = 0;

	/* Clear handlers, the connection to the previous RR really timed
	 * out and doesn't interest us anymore. */
	if (conn_socket->fd >= 0)
		clear_handlers(conn_socket->fd);

	for (i = conn_info->triedno + 1; i < conn_info->addrno; i++) {
#ifdef CONFIG_IPV6
		struct sockaddr_in6 addr = *((struct sockaddr_in6 *) &conn_info->addr[i]);
#else
		struct sockaddr_in addr = *((struct sockaddr_in *) &conn_info->addr[i]);
#endif
		int family;
		int force_family = conn_info->ip_family;

#ifdef CONFIG_IPV6
		family = addr.sin6_family;
#else
		family = addr.sin_family;
#endif

		conn_info->triedno++;

		if (only_local) {
			int local = 0;
#ifdef CONFIG_IPV6
			if (addr.sin6_family == AF_INET6)
				local = check_if_local_address6((struct sockaddr_in6 *) &addr);
			else
#endif
				local = check_if_local_address4((struct sockaddr_in *) &addr);

			/* This forbids connections to anything but local, if option is set. */
			if (!local) {
				at_least_one_remote_ip = 1;
				continue;
			}
		}

#ifdef CONFIG_IPV6
		if (family == AF_INET6 && (!get_opt_bool("connection.try_ipv6") || (force_family && force_family != 6))) {
			silent_fail = 1;
			continue;
		} else
#endif
		if (family == AF_INET && (!get_opt_bool("connection.try_ipv4") || (force_family && force_family != 4))) {
			silent_fail = 1;
			continue;
		}
		silent_fail = 0;

		sock = socket(family, SOCK_STREAM, IPPROTO_TCP);
		if (sock == -1) {
			if (errno && !saved_errno) saved_errno = errno;
			continue;
		}

		if (set_nonblocking_fd(sock) < 0) {
			if (errno && !saved_errno) saved_errno = errno;
			close(sock);
			continue;
		}
		conn_socket->fd = sock;

#ifdef CONFIG_IPV6
		addr.sin6_port = htons(conn_info->port);
#else
		addr.sin_port = htons(conn_info->port);
#endif

		/* We can set conn_socket->protocol_family here even if the connection
		 * will fail, as we will use it only when it will be successfully
		 * established. At least I hope that noone else will want to do
		 * something else ;-). --pasky */

#ifdef CONFIG_IPV6
		if (addr.sin6_family == AF_INET6) {
			conn_socket->protocol_family = 1;
			if (connect(sock, (struct sockaddr *) &addr,
					sizeof(struct sockaddr_in6)) == 0)
				break;
		} else
#endif
		{
			conn_socket->protocol_family = 0;
			if (connect(sock, (struct sockaddr *) &addr,
					sizeof(struct sockaddr_in)) == 0)
				break; /* success */
		}

		if (errno == EALREADY
#ifdef EWOULDBLOCK
		    || errno == EWOULDBLOCK
#endif
		    || errno == EINPROGRESS) {
			/* It will take some more time... */
			set_handlers(sock, NULL, (select_handler_T) connected,
				     (select_handler_T) dns_exception, conn_socket);
			conn_socket->ops->set_state(conn_socket->conn, conn_socket, S_CONN);
			return;
		}

		if (errno && !saved_errno) saved_errno = errno;

		close(sock);
	}

	if (i >= conn_info->addrno) {
		enum connection_state state;

		/* Tried everything, but it didn't help :(. */

		if (only_local && !saved_errno && at_least_one_remote_ip) {
			/* Yes we might hit a local address and fail in the
			 * process, but what matters is the last one because
			 * we do not know the previous one's errno, and the
			 * added complexity wouldn't really be worth it. */
			conn_socket->ops->done(conn_socket->conn, conn_socket, S_LOCAL_ONLY);
			return;
		}

		/* Retry reporting the errno state only if we already tried
		 * something new. Else use the S_DNS _progress_ state to make
		 * sure that no download callbacks will report any errors. */
		if (trno != conn_info->triedno && !silent_fail)
			state = -errno;
		else
			state = S_DNS;

		conn_socket->ops->retry(conn_socket->conn, conn_socket, state);
		return;
	}

#ifdef CONFIG_SSL
	/* Check if the connection should run over an encrypted link */
	if (conn_info->need_ssl
	    && ssl_connect(conn_socket) < 0)
		return;
#endif

	done_connection_info(conn_socket);
}

static void
connected(struct socket *socket)
{
	struct conn_info *conn_info = socket->conn_info;
	int err = 0;
	int len = sizeof(err);

	assertm(conn_info, "Lost conn_info!");
	if_assert_failed return;

	if (getsockopt(socket->fd, SOL_SOCKET, SO_ERROR, (void *) &err, &len) == 0) {
		/* Why does EMX return so large values? */
		if (err >= 10000) err -= 10000;
	} else {
		/* getsockopt() failed */
		if (errno > 0)
			err = errno;
		else
			err = -(S_STATE);
	}

	if (err > 0) {
		socket->ops->set_state(socket->conn, socket, -err);

		/* There are maybe still some more candidates. */
		close_socket(socket);
		connect_socket(socket);
		return;
	}

#ifdef CONFIG_SSL
	/* Check if the connection should run over an encrypted link */
	if (conn_info->need_ssl
	    && ssl_connect(socket) < 0)
		return;
#endif

	done_connection_info(socket);
}


struct write_buffer {
	/* A routine called when all the data is sent (therefore this is
	 * _different_ from read_buffer.done !). */
	socket_write_operation_T done;

	int len;
	int pos;

	unsigned char data[1]; /* must be at end of struct */
};

static int
generic_write(struct socket *socket, unsigned char *data, int len)
{
	int wr = safe_write(socket->fd, data, len);

	if (!wr) return SOCKET_CANT_WRITE;

	return wr < 0 ? SOCKET_SYSCALL_ERROR : wr;
}

static void
write_select(struct socket *socket)
{
	struct write_buffer *wb = socket->buffer;
	int wr;

	assertm(wb, "write socket has no buffer");
	if_assert_failed {
		socket->ops->done(socket->conn, socket, S_INTERNAL);
		return;
	}

	/* We are making some progress, therefore reset the timeout; ie.  when
	 * uploading large files the time needed for all the data to be sent
	 * can easily exceed the timeout. We don't need to do this for
	 * read_select() because it calls user handler every time new data is
	 * acquired and the user handler does this. */
	socket->ops->set_timeout(socket->conn, socket, 0);

#if 0
	printf("ws: %d\n",wb->len-wb->pos);
	for (wr = wb->pos; wr < wb->len; wr++) printf("%c", wb->data[wr]);
	printf("-\n");
#endif

#ifdef CONFIG_SSL
	if (socket->ssl) {
		wr = ssl_write(socket, wb->data + wb->pos, wb->len - wb->pos);
	} else
#endif
	{
		assert(wb->len - wb->pos > 0);
		wr = generic_write(socket, wb->data + wb->pos, wb->len - wb->pos);
	}

	switch (wr) {
	case SOCKET_CANT_WRITE:
		socket->ops->retry(socket->conn, socket, S_CANT_WRITE);
		break;

	case SOCKET_SYSCALL_ERROR:
		socket->ops->retry(socket->conn, socket, -errno);
		break;

	case SOCKET_INTERNAL_ERROR:
		/* The global errno variable is used for passing
		 * internal connection_state error value. */
		socket->ops->done(socket->conn, socket, -errno);
		break;

	default:
		if (wr < 0) break;

		/*printf("wr: %d\n", wr);*/
		wb->pos += wr;

		if (wb->pos == wb->len) {
			socket_write_operation_T done = wb->done;

			clear_handlers(socket->fd);
			mem_free_set(&socket->buffer, NULL);
			done(socket->conn, socket);
		}
	}
}

void
write_to_socket(struct socket *socket, unsigned char *data, int len,
		int connection_state, socket_write_operation_T write_done)
{
	struct write_buffer *wb;

	debug_transfer_log(data, len);

	assert(len > 0);
	if_assert_failed return;

	socket->ops->set_timeout(socket->conn, socket, 0);

	wb = mem_alloc(sizeof(*wb) + len);
	if (!wb) {
		socket->ops->done(socket->conn, socket, S_OUT_OF_MEM);
		return;
	}

	wb->len = len;
	wb->pos = 0;
	wb->done = write_done;
	memcpy(wb->data, data, len);
	mem_free_set(&socket->buffer, wb);
	set_handlers(socket->fd, NULL, (select_handler_T) write_select,
		     (select_handler_T) exception, socket);
	socket->ops->set_state(socket->conn, socket, connection_state);
}

#define RD_ALLOC_GR (2<<11) /* 4096 */
#define RD_MEM(rb) (sizeof(*(rb)) + 4 * RD_ALLOC_GR + RD_ALLOC_GR)
#define RD_SIZE(rb, len) ((RD_MEM(rb) + (len)) & ~(RD_ALLOC_GR - 1))

static int
generic_read(struct socket *socket, unsigned char *data, int len)
{
	int rd = safe_read(socket->fd, data, len);

	if (!rd) return SOCKET_CANT_READ;

	return rd < 0 ? SOCKET_SYSCALL_ERROR : rd;
}

static void
read_select(struct socket *socket)
{
	struct read_buffer *rb = socket->buffer;
	int rd;

	assertm(rb, "read socket has no buffer");
	if_assert_failed {
		socket->ops->done(socket->conn, socket, S_INTERNAL);
		return;
	}

	/* XXX: Should we call socket->set_timeout() as we do in write_select()?
	 * --pasky */

	clear_handlers(socket->fd);

	if (!rb->freespace) {
		int size = RD_SIZE(rb, rb->len);

		rb = mem_realloc(rb, size);
		if (!rb) {
			socket->ops->done(socket->conn, socket, S_OUT_OF_MEM);
			return;
		}
		rb->freespace = size - sizeof(*rb) - rb->len;
		assert(rb->freespace > 0);
		socket->buffer = rb;
	}

#ifdef CONFIG_SSL
	if (socket->ssl) {
		rd = ssl_read(socket, rb->data + rb->len, rb->freespace);
	} else
#endif
	{
		rd = generic_read(socket, rb->data + rb->len, rb->freespace);
	}

	switch (rd) {
#ifdef CONFIG_SSL
	case SOCKET_SSL_WANT_READ:
		read_from_socket(socket, rb, S_SSL_NEG, rb->done);
		break;
#endif
	case SOCKET_CANT_READ:
		if (socket->state != SOCKET_RETRY_ONCLOSE) {
			socket->state = SOCKET_CLOSED;
			rb->done(socket->conn, socket, rb);
			break;
		}

		errno = -S_CANT_READ;
		/* Fall-through */

	case SOCKET_SYSCALL_ERROR:
		socket->ops->retry(socket->conn, socket, -errno);
		break;

	case SOCKET_INTERNAL_ERROR:
		socket->ops->done(socket->conn, socket, -errno);
		break;

	default:
		debug_transfer_log(rb->data + rb->len, rd);

		rb->len += rd;
		rb->freespace -= rd;
		assert(rb->freespace >= 0);

		rb->done(socket->conn, socket, rb);
	}
}

struct read_buffer *
alloc_read_buffer(struct socket *socket)
{
	struct read_buffer *rb;

	rb = mem_calloc(1, RD_SIZE(rb, 0));
	if (!rb) {
		socket->ops->done(socket->conn, socket, S_OUT_OF_MEM);
		return NULL;
	}

	rb->freespace = RD_SIZE(rb, 0) - sizeof(*rb);

	return rb;
}

#undef RD_ALLOC_GR
#undef RD_MEM
#undef RD_SIZE

void
read_from_socket(struct socket *socket, struct read_buffer *buffer,
		 int connection_state, socket_read_operation_T done)
{
	buffer->done = done;

	socket->ops->set_timeout(socket->conn, socket, 0);
	socket->ops->set_state(socket->conn, socket, connection_state);

	if (socket->buffer && buffer != socket->buffer)
		mem_free(socket->buffer);
	socket->buffer = buffer;

	set_handlers(socket->fd, (select_handler_T) read_select, NULL,
		     (select_handler_T) exception, socket);
}

static void
read_response_from_socket(struct connection *conn, struct socket *socket)
{
	struct read_buffer *rb = alloc_read_buffer(socket);

	if (rb) read_from_socket(socket, rb, S_SENT, socket->read_done);
}

void
request_from_socket(struct socket *socket, unsigned char *data, int datalen,
		    int connection_state, enum socket_state state,
		    socket_read_operation_T read_done)
{
	socket->read_done = read_done;
	socket->state = state;
	write_to_socket(socket, data, datalen, connection_state,
			read_response_from_socket);
}

void
kill_buffer_data(struct read_buffer *rb, int n)
{
	assertm(n >= 0 && n <= rb->len, "bad number of bytes: %d", n);
	if_assert_failed { rb->len = 0;  return; }

	if (!n) return; /* FIXME: We accept to kill 0 bytes... */
	rb->len -= n;
	memmove(rb->data, rb->data + n, rb->len);
	rb->freespace += n;
}
