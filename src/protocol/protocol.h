/* $Id: protocol.h,v 1.2 2003/06/26 18:34:38 jonas Exp $ */

#ifndef EL__PROTOCOL_PROTOCOL_H
#define EL__PROTOCOL_PROTOCOL_H

#include "sched/sched.h"
#include "sched/session.h"

struct protocol_backend {
	unsigned char *name;
	int port;
	void (*func)(struct connection *);
	void (*nc_func)(struct session *, unsigned char *);
	int free_syntax;
	int need_slashes;
	int need_slash_after_host;
};

int check_protocol(unsigned char *p, int l);
void (*get_protocol_handle(unsigned char *))(struct connection *);
void (*get_external_protocol_function(unsigned char *))(struct session *, unsigned char *);
void get_prot_url_info(int i, int *free_syntax, int *need_slashes, int *need_slash_after_host);
int get_prot_info(unsigned char *prot, int *port,
		  void (**func)(struct connection *),
		  void (**nc_func)(struct session *ses, unsigned char *));

#endif
