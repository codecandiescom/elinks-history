/* $Id: auth.h,v 1.15 2003/12/20 16:59:02 pasky Exp $ */

#ifndef EL__PROTOCOL_AUTH_AUTH_H
#define EL__PROTOCOL_AUTH_AUTH_H

#include "protocol/uri.h"

struct http_auth_basic {
	LIST_HEAD(struct http_auth_basic);

	unsigned char *url;
	unsigned char *realm;
	unsigned char *user;
	unsigned char *password;
	unsigned int blocked:1;
	unsigned int valid:1;
};

#define auth_entry_has_userinfo(_entry_) \
	(*(_entry_)->user && *(_entry_)->password)

unsigned char *find_auth(struct uri *);
struct http_auth_basic *add_auth_entry(struct uri *, unsigned char *);
void del_auth_entry(struct http_auth_basic *);
void free_auth(void);
struct http_auth_basic *get_invalid_auth_entry(void);

#endif
