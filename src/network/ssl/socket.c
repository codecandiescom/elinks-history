/* SSL socket workshop */
/* $Id: socket.c,v 1.4 2002/05/08 13:55:06 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#ifdef HAVE_SSL
#include <openssl/ssl.h>
#endif

#include "links.h"

#include "lowlevel/connect.h"
#include "lowlevel/dns.h"
#include "ssl/connect.h"
#include "ssl/ssl.h"
#include "lowlevel/select.h"
#include "lowlevel/sched.h"


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

/* Return -1 on error, 0 or success. */
int ssl_connect(struct connection *conn, int sock)
{
        struct conn_info *c_i = (struct conn_info *) conn->buffer;

	conn->ssl = getSSL();
	SSL_set_fd(conn->ssl, sock);
	if (conn->no_tsl) conn->ssl->options |= SSL_OP_NO_TLSv1;

	switch (SSL_get_error(conn->ssl, SSL_connect(conn->ssl))) {
		case SSL_ERROR_WANT_READ:
			setcstate(conn, S_SSL_NEG);
			set_handlers(sock, (void (*)(void *)) ssl_want_read,
				     NULL, dns_exception, conn);
			return -1;

		case SSL_ERROR_NONE:
			break;

		default:
			conn->no_tsl++;
			setcstate(conn, S_SSL_ERROR);
			close_socket(c_i->sock);
			dns_found(conn, 0);
			return -1;
	}

	return 0;
}

/* Return -1 on error, wr or success. */
int ssl_write(struct connection *conn, struct write_buffer *wb)
{
	int wr;

	wr = SSL_write(conn->ssl, wb->data + wb->pos,
		       wb->len - wb->pos);
	
	if (wr <= 0) {
		int err = SSL_get_error(conn->ssl, wr);
		
		if (err == SSL_ERROR_WANT_WRITE) {
			return -1;
		}
		
		setcstate(conn, wr ? (err == SSL_ERROR_SYSCALL ? -errno : S_SSL_ERROR) : S_CANT_WRITE);
		
		if (!wr || err == SSL_ERROR_SYSCALL)
			retry_connection(conn);
		else
			abort_connection(conn);
		
		return -1;
	}

	return wr;
}

/* Return -1 on error, rd or success. */
int ssl_read(struct connection *conn, struct read_buffer *rb)
{
	int rd;

	rd = SSL_read(conn->ssl, rb->data + rb->len, READ_SIZE);
	
	if (rd <= 0) {
		int err = SSL_get_error(conn->ssl, rd);
		
		if (err == SSL_ERROR_WANT_READ) {
			read_from_socket(conn, rb->sock, rb, rb->done);
			return -1;
		}
		
		if (rb->close && !rd) {
			rb->close = 2;
			rb->done(conn, rb);
			return -1;
		}
		
		setcstate(conn, rd ? (err == SSL_ERROR_SYSCALL ? -errno : S_SSL_ERROR) : S_CANT_READ);
		
		/* mem_free(rb); */
		
		if (!rd || err == SSL_ERROR_SYSCALL)
			retry_connection(conn);
		else
			abort_connection(conn);
		
		return -1;
	}

	return rd;
}

#endif
