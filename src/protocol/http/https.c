/* Internal "https" protocol implementation */
/* $Id: https.c,v 1.23 2004/05/07 17:47:05 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "protocol/http/http.h"
#include "protocol/http/https.h"
#include "protocol/protocol.h"
#include "sched/connection.h"
#include "ssl/ssl.h"

void
https_protocol_handler(struct connection *conn)
{
	if (init_ssl_connection(conn) == S_SSL_ERROR)
		abort_conn_with_state(conn, S_SSL_ERROR);
	else
		http_protocol_handler(conn);
}
