/* $Id: protocol.h,v 1.11 2003/06/26 23:49:00 jonas Exp $ */

#ifndef EL__PROTOCOL_PROTOCOL_H
#define EL__PROTOCOL_PROTOCOL_H

#include "sched/sched.h"
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

enum protocol check_protocol(unsigned char *p, int l);

protocol_handler *get_protocol_handler(unsigned char *);
protocol_external_handler *get_protocol_external_handler(unsigned char *);

/* Accessors for protocol backends. */
int get_protocol_port(enum protocol protocol);
int get_protocol_free_syntax(enum protocol protocol);
int get_protocol_need_slashes(enum protocol protocol);
int get_protocol_need_slash_after_host(enum protocol protocol);

int get_prot_info(unsigned char *prot, int *port, protocol_handler **handler,
		  protocol_external_handler **external_handler);

#endif
