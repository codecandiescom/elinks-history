/* $Id: protocol.h,v 1.24 2004/04/11 15:32:22 jonas Exp $ */

#ifndef EL__PROTOCOL_PROTOCOL_H
#define EL__PROTOCOL_PROTOCOL_H

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
	PROTOCOL_LUA,
	PROTOCOL_PROXY,

	/* Keep these two last! */
	PROTOCOL_UNKNOWN,
	PROTOCOL_USER,
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

#endif
