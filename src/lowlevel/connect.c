/* Sockets-o-matic */
/* $Id: connect.c,v 1.55 2004/01/31 00:37:28 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h> /* OS/2 needs this after sys/types.h */
#include <sys/socket.h> /* OS/2 needs this after sys/types.h */
#ifdef HAVE_FCNTL_H
#include <fcntl.h> /* OS/2 needs this after sys/types.h */
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

/* This is for some exotic TOS mangling when creating passive FTP sockets. */
#ifdef HAVE_NETINET_IN_SYSTM_H
#include <netinet/in_systm.h>
#else
#ifdef HAVE_NETINET_IN_SYSTEM_H
#include <netinet/in_system.h>
#endif
#endif
#ifdef HAVE_NETINET_IP_H
#include <netinet/ip.h>
#endif

#include "elinks.h"

#include "config/options.h"
#include "lowlevel/connect.h"
#include "lowlevel/dns.h"
#include "lowlevel/select.h"
#include "osdep/osdep.h"
#include "protocol/uri.h"
#include "sched/connection.h"
#include "ssl/connect.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"

/*
#define LOG_TRANSFER	"/tmp/log"
*/

#ifdef LOG_TRANSFER
void
log_data(unsigned char *data, int len)
{
	int fd = open(LOG_TRANSFER, O_WRONLY | O_APPEND | O_CREAT, 0622);

	if (fd != -1) {
		set_bin(fd);
		write(fd, data, len);
		close(fd);
	}
}

#else
#define log_data(x, y)
#endif

void dns_found(/* struct connection */ void *, int);
void connected(/* struct connection */ void *);

void
close_socket(struct connection *conn, int *s)
{
	if (*s == -1) return;
#ifdef HAVE_SSL
	if (conn && conn->ssl) ssl_close(conn);
#endif
	close(*s);
	set_handlers(*s, NULL, NULL, NULL, NULL);
	*s = -1;
}

void
dns_exception(void *data)
{
	struct connection *conn = data;

	set_connection_state(conn, S_EXCEPT);
	close_socket(NULL, conn->conn_info->sock);
	dns_found(conn, 0);
}

static void
exception(void *data)
{
	retry_conn_with_state((struct connection *) data, S_EXCEPT);
}

void
make_connection(struct connection *conn, int port, int *sock,
		void (*func)(struct connection *))
{
	unsigned char *host = memacpy(conn->uri.host, conn->uri.hostlen);
	struct conn_info *c_i;
	int async;

	if (!host) {
		retry_conn_with_state(conn, S_OUT_OF_MEM);
		return;
	}

	c_i = mem_calloc(1, sizeof(struct conn_info));
	if (!c_i) {
		mem_free(host);
		retry_conn_with_state(conn, S_OUT_OF_MEM);
		return;
	}

	c_i->func = func;
	c_i->sock = sock;
	c_i->port = port;
	c_i->triedno = -1;
	c_i->addr = NULL;
	conn->conn_info = c_i;

	log_data("\nCONNECTION: ", 13);
	log_data(host, strlen(host));
	log_data("\n", 1);

	if (conn->cache_mode >= CACHE_MODE_FORCE_RELOAD)
		async = find_host_no_cache(host, &c_i->addr, &c_i->addrno,
					   &conn->dnsquery, dns_found, conn);
	else
		async = find_host(host, &c_i->addr, &c_i->addrno,
				  &conn->dnsquery, dns_found, conn);

	mem_free(host);

	if (async) set_connection_state(conn, S_DNS);
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
		retry_conn_with_state(conn, -errno);
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

	memcpy(&sb, &sa, sizeof(struct sockaddr_in));
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

#if defined(IP_TOS) && defined(IPTOS_THROUGHPUT)
	{
		int on = IPTOS_THROUGHPUT;
		setsockopt(sock, IPPROTO_IP, IP_TOS, (char *)&on, sizeof(int));
	}
#endif

	return sock;
}

#ifdef IPV6
int
get_pasv6_socket(struct connection *conn, int ctrl_sock,
		 struct sockaddr_storage *s6)
{
	int sock;
	struct sockaddr_in6 s0;
	int len = sizeof(struct sockaddr_in6);

	memset(&s0, 0, sizeof(s0));
	memset(s6, 0, sizeof(s6));

	/* Get our endpoint of the control socket */

	if (getsockname(ctrl_sock, (struct sockaddr *) s6, &len)) {
sock_error:
		retry_conn_with_state(conn, -errno);
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

	memcpy(&s0, s6, sizeof(struct sockaddr_in6));
	s0.sin6_port = 0;
	if (bind(sock, (struct sockaddr *) &s0, sizeof(struct sockaddr_in6)))
		goto sock_error;

	/* Get our endpoint of the passive socket and save it to port */

	len = sizeof(struct sockaddr_in6);
	if (getsockname(sock, (struct sockaddr *) s6, &len))
		goto sock_error;

	/* Go listen */

	if (listen(sock, 1))
		goto sock_error;

#if defined(IP_TOS) && defined(IPTOS_THROUGHPUT)
	{
		int on = IPTOS_THROUGHPUT;
		setsockopt(sock, IPPROTO_IP, IP_TOS, (char *)&on, sizeof(int));
	}
#endif

	return sock;
}
#endif


void
dns_found(void *data, int state)
{
	int sock = -1;
	struct connection *conn = (struct connection *) data;
	struct conn_info *c_i = conn->conn_info;
	int i;
	int trno = c_i->triedno;
	int not_local_count = trno;
	int only_local = get_opt_int_tree(cmdline_options, "localhost");
	
	if (state < 0) {
		abort_conn_with_state(conn, S_NO_DNS);
		return;
	}

	/* Clear handlers, the connection to the previous RR really timed
	 * out and doesn't interest us anymore. */
	if (c_i->sock && *c_i->sock >= 0)
		set_handlers(*c_i->sock, NULL, NULL, NULL, conn);

	for (i = c_i->triedno + 1; i < c_i->addrno; i++) {
#ifdef IPV6
		struct sockaddr_in6 addr = *((struct sockaddr_in6 *)&c_i->addr[i]);
#else
		struct sockaddr_in addr = *((struct sockaddr_in *) &c_i->addr[i]);
#endif
		int local = 0;

		c_i->triedno++;

		if (only_local) {
#ifdef IPV6
			if (addr.sin6_family == AF_INET6)
				local = IN6_IS_ADDR_LOOPBACK(&(((struct sockaddr_in6 *) &addr)->sin6_addr));
			else
#endif
				local = (ntohl(((struct sockaddr_in *) &addr)->sin_addr.s_addr) >> 24)
					== IN_LOOPBACKNET;
			
			/* This forbids connections to anything but local, if option is set. */
			if (!local) continue;
			
			/* Count external attempts. */
			not_local_count += !local;
		}

#ifdef IPV6
		sock = socket(addr.sin6_family, SOCK_STREAM, IPPROTO_TCP);
#else
		sock = socket(addr.sin_family, SOCK_STREAM, IPPROTO_TCP);
#endif
		if (sock == -1) continue;
		if (set_nonblocking_fd(sock) < 0) {
			close(sock);
			continue;
		}
		*c_i->sock = sock;

#ifdef IPV6
		addr.sin6_port = htons(c_i->port);
#else
		addr.sin_port = htons(c_i->port);
#endif

		/* We can set conn->pf here even if the connection will fail,
		 * as we will use it only when it will be successfully
		 * established. At least I hope that noone else will want to do
		 * something else ;-). --pasky */

#ifdef IPV6
		if (addr.sin6_family == AF_INET6) {
			conn->pf = 2;
			if (connect(sock, (struct sockaddr *) &addr,
					sizeof(struct sockaddr_in6)) == 0)
				break;
		} else
#endif
		{
			conn->pf = 1;
			if (connect(sock, (struct sockaddr *) &addr,
					sizeof(struct sockaddr_in)) == 0)
				break; /* success */
		}

		if (errno == EALREADY || errno == EINPROGRESS) {
			/* It will take some more time... */
			set_handlers(sock, NULL, connected, dns_exception, conn);
			set_connection_state(conn, S_CONN);
			return;
		}

		close(sock);
	}

	if (i >= c_i->addrno) {
		/* Tried everything, but it didn't help :(. */

		if (only_local && not_local_count >= c_i->triedno - 1) {
			abort_conn_with_state(conn, S_LOCAL_ONLY);
			return;
		}

		/* We set new state only if we already tried something new. */
		if (trno != c_i->triedno) set_connection_state(conn, -errno);
		retry_connection(conn);
		return;
	}

#ifdef HAVE_SSL
	if (conn->ssl && ssl_connect(conn, sock) < 0) return;
#endif

	conn->conn_info = NULL;
	c_i->func(conn);
	if (c_i->addr) mem_free(c_i->addr);
	mem_free(c_i);
}

void
connected(void *data)
{
	struct connection *conn = (struct connection *) data;
	struct conn_info *c_i = conn->conn_info;
	void (*func)(struct connection *) = c_i ? c_i->func : NULL;
	int err = 0;
	int len = sizeof(int);

	assertm(c_i, "Lost conn_info!");
	if_assert_failed return;

	if (getsockopt(*c_i->sock, SOL_SOCKET, SO_ERROR, (void *) &err, &len) == 0) {
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
		set_connection_state(conn, -err);

		/* There are maybe still some more candidates. */
		close_socket(NULL, c_i->sock);
		dns_found(conn, 0);
		return;
	}

#ifdef HAVE_SSL
	if (conn->ssl && ssl_connect(conn, *c_i->sock) < 0) return;
#endif

	conn->conn_info = NULL;
	func(conn);
	if (c_i->addr) mem_free(c_i->addr);
	mem_free(c_i);
}

static void
write_select(struct connection *conn)
{
	struct write_buffer *wb = conn->buffer;
	int wr;

	assertm(wb, "write socket has no buffer");
	if_assert_failed {
		abort_conn_with_state(conn, S_INTERNAL);
		return;
	}

#if 0
	printf("ws: %d\n",wb->len-wb->pos);
	for (wr = wb->pos; wr < wb->len; wr++) printf("%c", wb->data[wr]);
	printf("-\n");
#endif

#ifdef HAVE_SSL
	if (conn->ssl) {
		wr = ssl_write(conn, wb);
		if (wr <= 0) return;
	} else
#endif
	{
		assert(wb->len - wb->pos > 0);
		wr = safe_write(wb->sock, wb->data + wb->pos, wb->len - wb->pos);
		if (wr <= 0) {
			retry_conn_with_state(conn, wr ? -errno : S_CANT_WRITE);
			return;
		}
	}

	/*printf("wr: %d\n", wr);*/
	wb->pos += wr;
	if (wb->pos == wb->len) {
		void (*f)(struct connection *) = wb->done;

		conn->buffer = NULL;
		set_handlers(wb->sock, NULL, NULL, NULL, NULL);
		mem_free(wb);
		f(conn);
	}
}

void
write_to_socket(struct connection *conn, int s, unsigned char *data,
		int len, void (*write_func)(struct connection *))
{
	struct write_buffer *wb;

	log_data(data, len);

	assert(len > 0);
	if_assert_failed return;

	wb = mem_alloc(sizeof(struct write_buffer) + len);
	if (!wb) {
		abort_conn_with_state(conn, S_OUT_OF_MEM);
		return;
	}

	wb->sock = s;
	wb->len = len;
	wb->pos = 0;
	wb->done = write_func;
	memcpy(wb->data, data, len);
	if (conn->buffer) mem_free(conn->buffer);
	conn->buffer = wb;
	set_handlers(s, NULL, (void *)write_select, (void *)exception, conn);
}

#define RD_ALLOC_GR (2<<11) /* 4096 */
#define RD_MEM (sizeof(struct read_buffer) + 4 * RD_ALLOC_GR + RD_ALLOC_GR)
#define RD_SIZE(len) ((RD_MEM + (len)) & ~(RD_ALLOC_GR - 1))

static void
read_select(struct connection *conn)
{
	struct read_buffer *rb = conn->buffer;
	int rd;

	assertm(rb, "read socket has no buffer");
	if_assert_failed {
		abort_conn_with_state(conn, S_INTERNAL);
		return;
	}

	set_handlers(rb->sock, NULL, NULL, NULL, NULL);

	if (!rb->freespace) {
		int size = RD_SIZE(rb->len);

		rb = mem_realloc(rb, size);
		if (!rb) {
			abort_conn_with_state(conn, S_OUT_OF_MEM);
			return;
		}
		rb->freespace = size - sizeof(struct read_buffer);
		assert(rb->freespace > 0);
		conn->buffer = rb;
	}

#ifdef HAVE_SSL
	if (conn->ssl) {
		rd = ssl_read(conn, rb);
		if (rd <= 0) return;
	} else
#endif
	{
		rd = safe_read(rb->sock, rb->data + rb->len, rb->freespace);
		if (rd <= 0) {
			if (rb->close && !rd) {
				rb->close = 2;
				rb->done(conn, rb);
				return;
			}

			retry_conn_with_state(conn, rd ? -errno : S_CANT_READ);
			return;
		}
	}

	log_data(rb->data + rb->len, rd);

	rb->len += rd;
	rb->freespace -= rd;
	assert(rb->freespace >= 0);

	rb->done(conn, rb);
}

struct read_buffer *
alloc_read_buffer(struct connection *conn)
{
	struct read_buffer *rb;

	rb = mem_calloc(1, RD_SIZE(0));
	if (!rb) {
		abort_conn_with_state(conn, S_OUT_OF_MEM);
		return NULL;
	}

	rb->freespace = RD_SIZE(0) - sizeof(struct read_buffer);

	return rb;
}

#undef RD_ALLOC_GR
#undef RD_MEM
#undef RD_SIZE

void
read_from_socket(struct connection *conn, int s, struct read_buffer *buf,
		 void (*read_func)(struct connection *, struct read_buffer *))
{
	buf->done = read_func;
	buf->sock = s;
	if (conn->buffer && buf != conn->buffer)
		mem_free(conn->buffer);
	conn->buffer = buf;
	set_handlers(s, (void *)read_select, NULL, (void *)exception, conn);
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
