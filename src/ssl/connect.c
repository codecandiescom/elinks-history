/* SSL socket workshop */
/* $Id: connect.c,v 1.9 2002/07/05 03:59:40 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#ifdef HAVE_SSL
#ifdef HAVE_OPENSSL
#include <openssl/ssl.h>
#elif defined(HAVE_GNUTLS)
#include <gnutls.h>
#endif
#endif

#include "links.h"

#include "lowlevel/connect.h"
#include "lowlevel/dns.h"
#include "ssl/connect.h"
#include "ssl/ssl.h"
#include "lowlevel/select.h"
#include "lowlevel/sched.h"
#include "util/memory.h"


/* SSL errors */
#ifdef HAVE_SSL
#ifdef HAVE_OPENSSL
#define	SSL_ERROR_WANT_READ2	9999 /* XXX */
#define	SSL_ERROR_WANT_WRITE2	SSL_ERROR_WANT_WRITE
#define	SSL_ERROR_SYSCALL2	SSL_ERROR_SYSCALL
#elif defined(HAVE_GNUTLS)
#define	SSL_ERROR_NONE		GNUTLS_E_SUCCESS
#define	SSL_ERROR_WANT_READ	GNUTLS_E_AGAIN
#define	SSL_ERROR_WANT_READ2	GNUTLS_E_INTERRUPTED
#define	SSL_ERROR_WANT_WRITE	GNUTLS_E_AGAIN
#define	SSL_ERROR_WANT_WRITE2	GNUTLS_E_INTERRUPTED
#define	SSL_ERROR_SYSCALL	GNUTLS_E_PUSH_ERROR
#define	SSL_ERROR_SYSCALL2	GNUTLS_E_PULL_ERROR
#endif
#endif


static void
ssl_set_no_tls(struct connection *conn)
{
#ifdef HAVE_SSL
#ifdef HAVE_OPENSSL
	conn->ssl->options |= SSL_OP_NO_TLSv1;
#elif defined(HAVE_GNUTLS)
	int protocol_priority[];

	if (conn->no_tsl)
		protocol_priority = { GNUTLS_SSL3, 0 };
	else
		protocol_priority = { GNUTLS_TLS1, GNUTLS_SSL3, 0 };

	gnutls_protocol_set_priority(*conn->ssl, protocol_priority);
#endif
#endif
}


void
ssl_want_read(struct connection *conn)
{
#ifdef HAVE_SSL
	struct conn_info *b = conn->conn_info;

	if (conn->no_tsl)
		ssl_set_no_tls(conn);

	switch (
#ifdef HAVE_OPENSSL
		SSL_get_error(conn->ssl, SSL_connect(conn->ssl))
#elif defined(HAVE_GNUTLS)
		gnutls_handshake(*conn->ssl)
#endif
		) {
		case SSL_ERROR_NONE:
			conn->conn_info = NULL;
			b->func(conn);
			mem_free(b->addr);
			mem_free(b);

		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_READ2:
			break;

		default:
			conn->no_tsl++;
			setcstate(conn, S_SSL_ERROR);
			retry_connection(conn);
	}
#endif
	return;
}

/* Return -1 on error, 0 or success. */
int
ssl_connect(struct connection *conn, int sock)
{
#ifdef HAVE_SSL
        struct conn_info *c_i = (struct conn_info *) conn->buffer;

	conn->ssl = get_ssl();
	if (conn->no_tsl)
		ssl_set_no_tls(conn);
#ifdef HAVE_OPENSSL
	SSL_set_fd(conn->ssl, sock);
#elif defined(HAVE_GNUTLS)
	gnutls_transport_set_ptr(*conn->ssl, sock);
#endif

	switch (
#ifdef HAVE_OPENSSL
		SSL_get_error(conn->ssl, SSL_connect(conn->ssl))
#elif defined(HAVE_GNUTLS)
		gnutls_handshare(*conn->ssl)
#endif
		) {
		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_READ2:
			setcstate(conn, S_SSL_NEG);
			set_handlers(sock, (void (*)(void *)) ssl_want_read,
				     NULL, dns_exception, conn);
			return -1;

		case SSL_ERROR_NONE:
			break;

		default:
			conn->no_tsl++;
			setcstate(conn, S_SSL_ERROR);
			close_socket(NULL, c_i->sock);
			dns_found(conn, 0);
			return -1;
	}
#endif

	return 0;
}

/* Return -1 on error, wr or success. */
int
ssl_write(struct connection *conn, struct write_buffer *wb)
{
	int wr = -1;

#ifdef HAVE_SSL
#ifdef HAVE_OPENSSL
	wr = SSL_write(conn->ssl, wb->data + wb->pos,
		       wb->len - wb->pos);
#elif defined(HAVE_GNUTLS)
	wr = gnutls_record_send(*conn->ssl, wb->data + wb->pos,
				wb->len - wb->pos);
#endif

	if (wr <= 0) {
#ifdef HAVE_OPENSSL
		int err = SSL_get_error(conn->ssl, wr);
#elif defined(HAVE_GNUTLS)
		int err = wr;
#endif

		if (err == SSL_ERROR_WANT_WRITE ||
		    err == SSL_ERROR_WANT_WRITE2) {
			return -1;
		}

		setcstate(conn, wr ? (err == SSL_ERROR_SYSCALL ? -errno
							       : S_SSL_ERROR)
				   : S_CANT_WRITE);

		if (!wr || err == SSL_ERROR_SYSCALL)
			retry_connection(conn);
		else
			abort_connection(conn);

		return -1;
	}
#endif

	return wr;
}

/* Return -1 on error, rd or success. */
int
ssl_read(struct connection *conn, struct read_buffer *rb)
{
	int rd = -1;

#ifdef HAVE_SSL
#ifdef HAVE_OPENSSL
	rd = SSL_read(conn->ssl, rb->data + rb->len, READ_SIZE);
#elif defined(HAVE_GNUTLS)
	rd = gnutls_record_recv(*conn->ssl, rb->data + rb->len, READ_SIZE);
#endif

	if (rd <= 0) {
#ifdef HAVE_OPENSSL
		int err = SSL_get_error(conn->ssl, rd);
#elif defined(HAVE_GNUTLS)
		int err = rd;
#endif

#ifdef HAVE_GNUTLS
		if (err == GNUTLS_E_REHANDSHAKE)
			return -1;
#endif

		if (err == SSL_ERROR_WANT_READ ||
		    err == SSL_ERROR_WANT_READ2) {
			read_from_socket(conn, rb->sock, rb, rb->done);
			return -1;
		}

		if (rb->close && !rd) {
			rb->close = 2;
			rb->done(conn, rb);
			return -1;
		}

		setcstate(conn, rd ? (err == SSL_ERROR_SYSCALL2 ? -errno
								: S_SSL_ERROR)
				   : S_CANT_READ);

		/* mem_free(rb); */

		if (!rd || err == SSL_ERROR_SYSCALL2)
			retry_connection(conn);
		else
			abort_connection(conn);

		return -1;
	}
#endif

	return rd;
}

int
ssl_close(struct connection *conn)
{
#ifdef HAVE_SSL
#ifdef HAVE_OPENSSL
	/* Hmh? No idea.. */
#elif defined(HAVE_GNUTLS)
	/* We probably doesn't handle this entirely correctly.. */
	gnutls_bye(*conn->ssl, GNUTLS_SHUT_RDWR);
#endif
#endif
	return 0;
}
