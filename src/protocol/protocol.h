/* $Id: protocol.h,v 1.27 2004/05/07 17:30:01 jonas Exp $ */

#ifndef EL__PROTOCOL_PROTOCOL_H
#define EL__PROTOCOL_PROTOCOL_H

#include "modules/module.h"

struct connection;
struct session;
struct uri;

enum protocol {
	PROTOCOL_INVALID = -1,
	PROTOCOL_FILE,
	PROTOCOL_FINGER,
	PROTOCOL_FTP,
	PROTOCOL_HTTP,
	PROTOCOL_HTTPS,
	PROTOCOL_SMB,
	PROTOCOL_JAVASCRIPT,
	PROTOCOL_PROXY,

	/* Keep these last! */
	PROTOCOL_UNKNOWN,
	PROTOCOL_USER,
	PROTOCOL_LUA,
};

#define VALID_PROTOCOL(p) ((p) != PROTOCOL_INVALID)

/* Besides the session an external handler also takes the url as an argument */
typedef void (protocol_handler)(struct connection *);
typedef void (protocol_external_handler)(struct session *, struct uri *);

struct protocol_backend {
	unsigned char *name;
	int port;
	protocol_handler *handler;
	protocol_external_handler *external_handler;
	unsigned int free_syntax:1;
	unsigned int need_slashes:1;
	unsigned int need_slash_after_host:1;
};

extern struct protocol_backend unknown_protocol_backend;

/* Accessors for the protocol backends. */

int get_protocol_port(enum protocol protocol);
int get_protocol_free_syntax(enum protocol protocol);
int get_protocol_need_slashes(enum protocol protocol);
int get_protocol_need_slash_after_host(enum protocol protocol);

protocol_handler *get_protocol_handler(enum protocol protocol);
protocol_external_handler *get_protocol_external_handler(enum protocol protocol);

/* Resolves the given protocol @name with length @namelen to a known protocol,
 * PROTOCOL_UNKOWN or PROTOCOL_INVALID if no protocol part could be identified.
 * User defined protocols (configurable via protocol.user) takes precedence. */
enum protocol get_protocol(unsigned char *name, int namelen);

extern struct module protocol_module;

#endif
