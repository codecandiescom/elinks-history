/* SSL socket workshop */
/* $Id: connect.c,v 1.22 2003/05/08 23:03:08 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#ifdef HAVE_SSL
#ifdef HAVE_OPENSSL
#include <openssl/ssl.h>
#elif defined(HAVE_GNUTLS)
#include <gnutls/gnutls.h>
#endif
#endif

#include "elinks.h"

#include "config/options.h"
#include "lowlevel/connect.h"
#include "lowlevel/dns.h"
#include "ssl/connect.h"
#include "ssl/ssl.h"
#include "lowlevel/select.h"
#include "sched/sched.h"
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


#ifdef HAVE_SSL
static void
ssl_set_no_tls(struct connection *conn)
{
#ifdef HAVE_OPENSSL
	conn->ssl->options |= SSL_OP_NO_TLSv1;
#elif defined(HAVE_GNUTLS)
	/* We do a little more work here, setting up all these priorities (like
	 * they couldn't have some reasonable defaults there).. */

	{
		int protocol_priority[3];
		int i = 0;

		if (!conn->no_tsl)
			protocol_priority[i++] = GNUTLS_TLS1;
		protocol_priority[i++] = GNUTLS_SSL3;
		protocol_priority[i++] = 0;

		gnutls_protocol_set_priority(*conn->ssl, protocol_priority);
	}

	/* Note that I have no clue about these; I just put all I found here
	 * ;-). It is all a bit confusing for me, and I just want this to work.
	 * Feel free to send me patch removing useless superfluous bloat,
	 * thanks in advance. --pasky */

	{
		int cipher_priority[6] = {
			GNUTLS_CIPHER_RIJNDAEL_128_CBC,
			GNUTLS_CIPHER_3DES_CBC,
			GNUTLS_CIPHER_ARCFOUR,
			GNUTLS_CIPHER_TWOFISH_128_CBC,
			GNUTLS_CIPHER_RIJNDAEL_256_CBC,
			0
		};

		gnutls_cipher_set_priority(*conn->ssl, cipher_priority);
	}

	{
		/* Does any httpd support this..? ;) */
		int comp_priority[3] = {
			GNUTLS_COMP_ZLIB,
			GNUTLS_COMP_NULL,
			0
		};

		gnutls_compression_set_priority(*conn->ssl, comp_priority);
	}

	{
		int kx_priority[5] = {
			GNUTLS_KX_RSA,
			GNUTLS_KX_DHE_DSS,
			GNUTLS_KX_DHE_RSA,
			/* Looks like we don't want SRP, do we? */
			GNUTLS_KX_ANON_DH,
			0
		};

		gnutls_kx_set_priority(*conn->ssl, kx_priority);
	}

	{
		int mac_priority[3] = {
			GNUTLS_MAC_SHA,
			GNUTLS_MAC_MD5,
			0
		};

		gnutls_mac_set_priority(*conn->ssl, mac_priority);
	}

	{
		int cert_type_priority[2] = {
			GNUTLS_CRT_X509,
			/* We don't link with -extra now; by time of writing
			 * this, it's unclear where OpenPGP will end up. */
			0
		};

		gnutls_cert_type_set_priority(*conn->ssl, cert_type_priority);
	}

	gnutls_dh_set_prime_bits(*conn->ssl, 1024);
#endif
}
#endif

void ssl_want_read(struct connection *conn);

void
ssl_want_read(struct connection *conn)
{
#ifdef HAVE_SSL
	struct conn_info *b = conn->conn_info;

	if (!b) return;

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
#ifdef HAVE_GNUTLS
			if (get_opt_bool("connection.ssl.cert_verify"))
				if (gnutls_certificate_verify_peers(*conn->ssl))
					goto ssl_error;
#endif

			conn->conn_info = NULL;
			b->func(conn);
			if (b->addr) mem_free(b->addr);
			mem_free(b);

		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_READ2:
			break;

		default:
			conn->no_tsl++;
ssl_error:
			retry_conn_with_state(conn, S_SSL_ERROR);
	}
#endif
	return;
#ifdef HAVE_SSL
	goto ssl_error; /* XXX */
#endif
}

/* Return -1 on error, 0 or success. */
int
ssl_connect(struct connection *conn, int sock)
{
#ifdef HAVE_SSL
        struct conn_info *c_i = (struct conn_info *) conn->conn_info;
	int ret;

	conn->ssl = get_ssl();
	if (!conn->ssl)
		goto ssl_error;

	if (conn->no_tsl)
		ssl_set_no_tls(conn);

#ifdef HAVE_OPENSSL
	SSL_set_fd(conn->ssl, sock);
#elif defined(HAVE_GNUTLS)
	gnutls_transport_set_ptr(*conn->ssl, sock);
#endif

#ifdef HAVE_OPENSSL
	if (get_opt_bool("connection.ssl.cert_verify"))
		SSL_set_verify(conn->ssl, SSL_VERIFY_PEER
					  | SSL_VERIFY_FAIL_IF_NO_PEER_CERT,
				NULL);
	ret = SSL_get_error(conn->ssl, SSL_connect(conn->ssl));
#elif defined(HAVE_GNUTLS)
	ret = gnutls_handshake(*conn->ssl);
#endif

	switch (ret) {
		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_READ2:
			setcstate(conn, S_SSL_NEG);
			set_handlers(sock, (void (*)(void *)) ssl_want_read,
				     NULL, dns_exception, conn);
			return -1;

		case SSL_ERROR_NONE:
#ifdef HAVE_GNUTLS
			if (get_opt_bool("connection.ssl.cert_verify"))
				if (gnutls_certificate_verify_peers(*conn->ssl))
					goto ssl_error;
#endif
			break;

		default:
			/* debug("sslerr %s", gnutls_strerror(ret)); */
			conn->no_tsl++;
ssl_error:
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
	rd = SSL_read(conn->ssl, rb->data + rb->len, rb->freespace);
#elif defined(HAVE_GNUTLS)
	rd = gnutls_record_recv(*conn->ssl, rb->data + rb->len, rb->freespace);
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
	free_ssl(conn->ssl);
	conn->ssl = NULL;
#endif
	return 0;
}
