#include "links.h"

/*
#define LOG_TRANSFER	"/tmp/log"
*/

#ifdef LOG_TRANSFER
void log_data(unsigned char *data, int len)
{
	int fd;
	if ((fd = open(LOG_TRANSFER, O_WRONLY | O_APPEND | O_CREAT, 0622)) != -1) {
		set_bin(fd);
		write(fd, data, len);
		close(fd);
	}
}

#else
#define log_data(x, y)
#endif

void exception(void *data)
{
        struct connection *c = (struct connection *) data;
	
	setcstate(c, S_EXCEPT);
	retry_connection(c);
}

void close_socket(int *s)
{
	if (*s == -1) return;
	close(*s);
	set_handlers(*s, NULL, NULL, NULL, NULL);
	*s = -1;
}

void connected(/* struct connection */ void *);

struct conn_info {
	/* Note that we MUST start with addr - free_connection_info() relies on it */
	/* TODO: Remove this hack when we will have .h for each .c */
	struct sockaddr *addr; /* array of addresses */
	int addrno; /* array len / sizeof(sockaddr) */
	int port;
	struct sockaddr_in sa;
	int *sock;
	void (*func)(struct connection *);
};

void dns_found(/* struct connection */ void *, int);

void make_connection(struct connection *conn, int port, int *sock,
		     void (*func)(struct connection *))
{
	unsigned char *host;
	struct conn_info *c_i;
	int async;
	
	host = get_host_name(conn->url);
	if (!host) {
		setcstate(conn, S_INTERNAL);
		abort_connection(conn);
		return;
	}
	
	c_i = mem_alloc(sizeof(struct conn_info));
	if (!c_i) {
		mem_free(host);
		setcstate(conn, S_OUT_OF_MEM);
		retry_connection(conn);
		return;
	}
	
	c_i->func = func;
	c_i->sock = sock;
	c_i->port = port;
	conn->conn_info = c_i;
	
	log_data("\nCONNECTION: ", 13);
	log_data(host, strlen(host));
	log_data("\n", 1);
	
	if (conn->no_cache >= NC_RELOAD)
		async = find_host_no_cache(host, &c_i->addr, &c_i->addrno,
					   &conn->dnsquery, dns_found, conn);
	else
		async = find_host(host, &c_i->addr, &c_i->addrno,
				  &conn->dnsquery, dns_found, conn);
	
	mem_free(host);
	
	if (async) setcstate(conn, S_DNS);
}

int get_pasv_socket(struct connection *c, int cc, int *sock, unsigned char *port)
{
	int s;
	struct sockaddr_in sa;
	struct sockaddr_in sb;
	int len = sizeof(sa);
	if (getsockname(cc, (struct sockaddr *)&sa, &len)) {
		e:
		setcstate(c, -errno);
		retry_connection(c);
		return -2;
	}
	if ((s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) goto e;
	*sock = s;
	fcntl(s, F_SETFL, O_NONBLOCK);
	memcpy(&sb, &sa, sizeof(struct sockaddr_in));
	sb.sin_port = 0;
	if (bind(s, (struct sockaddr *)&sb, sizeof sb)) goto e;
	len = sizeof(sa);
	if (getsockname(s, (struct sockaddr *)&sa, &len)) goto e;
	if (listen(s, 1)) goto e;
	memcpy(port, &sa.sin_addr.s_addr, 4);
	memcpy(port + 4, &sa.sin_port, 2);
#if defined(IP_TOS) && defined(IPTOS_THROUGHPUT)
	{
		int on = IPTOS_THROUGHPUT;
		setsockopt(s, IPPROTO_IP, IP_TOS, (char *)&on, sizeof(int));
	}
#endif
	return 0;
}

#ifdef HAVE_SSL
void ssl_want_read(struct connection *c)
{
	struct conn_info *b = c->conn_info;

	if (c->no_tsl) c->ssl->options |= SSL_OP_NO_TLSv1;
	switch (SSL_get_error(c->ssl, SSL_connect(c->ssl))) {
		case SSL_ERROR_NONE:
			c->conn_info = NULL;
			b->func(c);
			mem_free(b->addr);
			mem_free(b);
		case SSL_ERROR_WANT_READ:
			break;
		default:
			c->no_tsl++;
			setcstate(c, S_SSL_ERROR);
			retry_connection(c);
	}
}
#endif

#ifdef HAVE_SSL
int ssl_connect(struct connection *conn, int sock)
{
	if (conn->ssl) {
		conn->ssl = getSSL();
		SSL_set_fd(conn->ssl, sock);
		if (conn->no_tsl) conn->ssl->options |= SSL_OP_NO_TLSv1;
		
		switch (SSL_get_error(conn->ssl, SSL_connect(conn->ssl))) {
			case SSL_ERROR_WANT_READ:
				setcstate(conn, S_SSL_NEG);
				set_handlers(sock, (void (*)(void *)) ssl_want_read,
					     NULL, exception, conn);
				return -1;
				
			case SSL_ERROR_NONE:
				break;
				
			default:
				conn->no_tsl++;
				setcstate(conn, S_SSL_ERROR);
				retry_connection(conn);
				return -1;
		}
	}

	return 0;
}
#endif

void dns_found(void *data, int state)
{
	int sock;
	struct connection *conn = (struct connection *) data;
	struct conn_info *c_i = conn->conn_info;
	int i;
	
	if (state < 0) {
		setcstate(conn, S_NO_DNS);
		retry_connection(conn);
		return;
	}
	
	sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock == -1) {
		setcstate(conn, -errno);
		retry_connection(conn);
		return;
	}
	
	*c_i->sock = sock;
	fcntl(sock, F_SETFL, O_NONBLOCK);

	memset(&c_i->sa, 0, sizeof(struct sockaddr_in));
	c_i->sa.sin_port = htons(c_i->port);

	for (i = 0; i < c_i->addrno; i++) {
		struct sockaddr_in addr = *((struct sockaddr_in *) &c_i->addr[i]);
		
		c_i->sa.sin_family = addr.sin_family;
		c_i->sa.sin_addr.s_addr = addr.sin_addr.s_addr;
		
		if (connect(sock, (struct sockaddr *) &c_i->sa, sizeof(c_i->sa)) == 0)
			break; /* success */
	}

	if (i == c_i->addrno) {
		/* tried everything, but didn't help :( */

		if (errno != EALREADY && errno != EINPROGRESS) {
			setcstate(conn, -errno);
			retry_connection(conn);
		} else {
			set_handlers(sock, NULL, connected, exception, conn);
			setcstate(conn, S_CONN);
		}
		
		return;
	}

#ifdef HAVE_SSL
	if (ssl_connect(conn, sock) < 0) return;
#endif
	conn->conn_info = NULL;
	c_i->func(conn);
	mem_free(c_i->addr);
	mem_free(c_i);
}

void connected(void *data)
{
        struct connection *conn = (struct connection *) data;
	struct conn_info *c_i = conn->conn_info;
	int err = 0;
	int len = sizeof(int);
	
	if (getsockopt(*c_i->sock, SOL_SOCKET, SO_ERROR, (void *)&err, &len))
		if (!(err = errno)) {
			err = -S_STATE;
			goto skiperrdec;
		}
	
	if (err >= 10000) err -= 10000;	/* Why does EMX return so large values? */
	
skiperrdec:
	if (err > 0) {
		setcstate(conn, -err);
		retry_connection(conn);
		
	} else {
		void (*func)(struct connection *) = c_i->func;

#ifdef HAVE_SSL
		if (ssl_connect(conn, *c_i->sock) < 0) return;
#endif
		conn->conn_info = NULL;
		func(conn);
		mem_free(c_i->addr);
		mem_free(c_i);
	}
}

struct write_buffer {
	int sock;
	int len;
	int pos;
	void (*done)(struct connection *);
	unsigned char data[1];
};

void write_select(struct connection *c)
{
	struct write_buffer *wb;
	int wr;
	if (!(wb = c->buffer)) {
		internal("write socket has no buffer");
		setcstate(c, S_INTERNAL);
		abort_connection(c);
		return;
	}
	/*printf("ws: %d\n",wb->len-wb->pos);
	for (wr = wb->pos; wr < wb->len; wr++) printf("%c", wb->data[wr]);
	printf("-\n");*/

#ifdef HAVE_SSL
	if(c->ssl) {
		if ((wr = SSL_write(c->ssl, wb->data + wb->pos, wb->len - wb->pos)) <= 0) {
			int err;
			if ((err = SSL_get_error(c->ssl, wr)) != SSL_ERROR_WANT_WRITE) {
				setcstate(c, wr ? (err == SSL_ERROR_SYSCALL ? -errno : S_SSL_ERROR) : S_CANT_WRITE);
				if (!wr || err == SSL_ERROR_SYSCALL) retry_connection(c);
				else abort_connection(c);
				return;
			}
			else return;
		}
	} else
#endif
		if ((wr = write(wb->sock, wb->data + wb->pos, wb->len - wb->pos)) <= 0) {
			setcstate(c, wr ? -errno : S_CANT_WRITE);
			retry_connection(c);
			return;
		}

	/*printf("wr: %d\n", wr);*/
	if ((wb->pos += wr) == wb->len) {
		void (*f)(struct connection *) = wb->done;
		c->buffer = NULL;
		set_handlers(wb->sock, NULL, NULL, NULL, NULL);
		mem_free(wb);
		f(c);
	}
}

void write_to_socket(struct connection *c, int s, unsigned char *data, int len, void (*write_func)(struct connection *))
{
	struct write_buffer *wb;
	log_data(data, len);
	if (!(wb = mem_alloc(sizeof(struct write_buffer) + len))) {
		setcstate(c, S_OUT_OF_MEM);
		abort_connection(c);
		return;
	}
	wb->sock = s;
	wb->len = len;
	wb->pos = 0;
	wb->done = write_func;
	memcpy(wb->data, data, len);
	if (c->buffer) mem_free(c->buffer);
	c->buffer = wb;
	set_handlers(s, NULL, (void (*)())write_select, (void (*)())exception, c);
}

#define READ_SIZE 16384

void read_select(struct connection *c)
{
	struct read_buffer *rb;
	int rd;
	if (!(rb = c->buffer)) {
		internal("read socket has no buffer");
		setcstate(c, S_INTERNAL);
		abort_connection(c);
		return;
	}
	set_handlers(rb->sock, NULL, NULL, NULL, NULL);
	if (!(rb = mem_realloc(rb, sizeof(struct read_buffer) + rb->len + READ_SIZE))) {
		setcstate(c, S_OUT_OF_MEM);
		abort_connection(c);
		return;
	}
	c->buffer = rb;

#ifdef HAVE_SSL
	if(c->ssl) {
		if ((rd = SSL_read(c->ssl, rb->data + rb->len, READ_SIZE)) <= 0) {
			int err;
			if ((err = SSL_get_error(c->ssl, rd)) == SSL_ERROR_WANT_READ) {
				read_from_socket(c, rb->sock, rb, rb->done);
				return;
			}
			if (rb->close && !rd) {
				rb->close = 2;
				rb->done(c, rb);
				return;
			}
			setcstate(c, rd ? (err == SSL_ERROR_SYSCALL ? -errno : S_SSL_ERROR) : S_CANT_READ);
			/*mem_free(rb);*/
			if (!rd || err == SSL_ERROR_SYSCALL) retry_connection(c);
			else abort_connection(c);
			return;
		}
	} else
#endif
		if ((rd = read(rb->sock, rb->data + rb->len, READ_SIZE)) <= 0) {
			if (rb->close && !rd) {
				rb->close = 2;
				rb->done(c, rb);
				return;
			}
			setcstate(c, rd ? -errno : S_CANT_READ);
			/*mem_free(rb);*/
			retry_connection(c);
			return;
		}
	log_data(rb->data + rb->len, rd);
	rb->len += rd;
	rb->done(c, rb);
}

struct read_buffer *alloc_read_buffer(struct connection *c)
{
	struct read_buffer *rb;
	if (!(rb = mem_alloc(sizeof(struct read_buffer) + READ_SIZE))) {
		setcstate(c, S_OUT_OF_MEM);
		abort_connection(c);
		return NULL;
	}
	memset(rb, 0, sizeof(struct read_buffer));
	return rb;
}

void read_from_socket(struct connection *c, int s, struct read_buffer *buf, void (*read_func)(struct connection *, struct read_buffer *))
{
	buf->done = read_func;
	buf->sock = s;
	if (c->buffer && buf != c->buffer) mem_free(c->buffer);
	c->buffer = buf;
	set_handlers(s, (void (*)())read_select, NULL, (void (*)())exception, c);
}

void kill_buffer_data(struct read_buffer *rb, int n)
{
	if (n > rb->len || n < 0) {
		internal("called kill_buffer_data with bad value");
		rb->len = 0;
		return;
	}
	memmove(rb->data, rb->data + n, rb->len - n);
	rb->len -= n;
}
