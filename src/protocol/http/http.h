/* $Id: http.h,v 1.21 2005/04/13 03:48:42 jonas Exp $ */

#ifndef EL__PROTOCOL_HTTP_HTTP_H
#define EL__PROTOCOL_HTTP_HTTP_H

#include "modules/module.h"
#include "protocol/protocol.h"

struct connection;
struct http_connection_info;
struct read_buffer;
struct socket;

extern struct module http_protocol_module;

extern protocol_handler_T http_protocol_handler;
extern protocol_handler_T proxy_protocol_handler;

#ifdef CONFIG_SSL
#define https_protocol_handler http_protocol_handler
#else
#define https_protocol_handler NULL
#endif

struct http_connection_info *init_http_connection_info(struct connection *conn, int major, int minor, int close);
void http_got_header(struct connection *, struct socket *, struct read_buffer *);

unsigned char *subst_user_agent(unsigned char *fmt, unsigned char *version,
				unsigned char *sysname, unsigned char *termsize);

#endif
