/* $Id: protocol.h,v 1.15 2003/07/04 15:01:29 jonas Exp $ */

#ifndef EL__PROTOCOL_PROTOCOL_H
#define EL__PROTOCOL_PROTOCOL_H

#include "sched/connection.h"
#include "sched/session.h"

enum protocol {
	PROTOCOL_FILE,
	PROTOCOL_FINGER,
	PROTOCOL_FTP,
	PROTOCOL_HTTP,
	PROTOCOL_HTTPS,
	PROTOCOL_JAVASCRIPT,
	PROTOCOL_LUA,
	PROTOCOL_PROXY,

	/* Keep these two last! */
	PROTOCOL_UNKNOWN,
	PROTOCOL_USER,
};

/* Besides the session an external handler also takes the url as an argument */
typedef void (protocol_handler)(struct connection *);
typedef void (protocol_external_handler)(struct session *, unsigned char *);

struct protocol_backend {
	unsigned char *name;
	int port;
	protocol_handler *handler;
	protocol_external_handler *external_handler;
	unsigned int free_syntax:1;
	unsigned int need_slashes:1;
	unsigned int need_slash_after_host:1;
};

/* Accessors for the protocol backends. */

int get_protocol_port(enum protocol protocol);
int get_protocol_free_syntax(enum protocol protocol);
int get_protocol_need_slashes(enum protocol protocol);
int get_protocol_need_slash_after_host(enum protocol protocol);

protocol_handler *get_protocol_handler(struct uri *uri);
protocol_external_handler *get_protocol_external_handler(unsigned char *url);

/* Resolves the given protocol @name to a known protocol or PROTOCOL_UNKOWN */
/* User defined protocols (configurable via protocol.user) takes precedence. */
enum protocol check_protocol(unsigned char *name, int namelen);

#endif
