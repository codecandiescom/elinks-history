/* $Id: protocol.h,v 1.8 2003/06/26 21:19:31 pasky Exp $ */

#ifndef EL__PROTOCOL_PROTOCOL_H
#define EL__PROTOCOL_PROTOCOL_H

#include "sched/sched.h"
#include "sched/session.h"

enum uri_scheme {
	SCHEME_FILE,
	SCHEME_FINGER,
	SCHEME_FTP,
	SCHEME_HTTP,
	SCHEME_HTTPS,
	SCHEME_JAVASCRIPT,
	SCHEME_LUA,
	SCHEME_PROXY,

	/* Keep these two last! */
	SCHEME_UNKNOWN,
	SCHEME_USER,
};

typedef void (protocol_handler)(struct connection *);
typedef void (protocol_external_handler)(struct session *, unsigned char *);

struct protocol_backend {
	unsigned char *name;
	int port;
	protocol_handler *handler;
	protocol_external_handler *external_handler;
	int free_syntax;
	int need_slashes;
	int need_slash_after_host;
};

enum uri_scheme check_protocol(unsigned char *p, int l);

protocol_handler *get_protocol_handler(unsigned char *);
protocol_external_handler *get_protocol_external_handler(unsigned char *);

/* Accessors for protocol backends. */
int get_protocol_free_syntax(enum uri_scheme scheme);
int get_protocol_need_slashes(enum uri_scheme scheme);
int get_protocol_need_slash_after_host(enum uri_scheme scheme);

int get_prot_info(unsigned char *prot, int *port, protocol_handler **handler,
		  protocol_external_handler **external_handler);

#endif
