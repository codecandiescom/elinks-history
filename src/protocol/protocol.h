/* $Id: protocol.h,v 1.5 2003/06/26 20:19:50 jonas Exp $ */

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

struct protocol_backend {
	unsigned char *name;
	int port;
	void (*func)(struct connection *);
	void (*nc_func)(struct session *, unsigned char *);
	int free_syntax;
	int need_slashes;
	int need_slash_after_host;
};

enum uri_scheme check_protocol(unsigned char *p, int l);
void (*get_protocol_handle(unsigned char *))(struct connection *);
void (*get_external_protocol_function(unsigned char *))(struct session *, unsigned char *);

/* Accessors for protocol backends. */
int get_protocol_free_syntax(enum uri_scheme scheme);
int get_protocol_need_slashes(enum uri_scheme scheme);
int get_protocol_need_slash_after_host(enum uri_scheme scheme);

int get_prot_info(unsigned char *prot, int *port,
		  void (**func)(struct connection *),
		  void (**nc_func)(struct session *ses, unsigned char *));

#endif
