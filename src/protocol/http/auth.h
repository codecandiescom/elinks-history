/* $Id: auth.h,v 1.12 2003/07/12 16:51:25 jonas Exp $ */

#ifndef EL__PROTOCOL_HTTP_AUTH_H
#define EL__PROTOCOL_HTTP_AUTH_H

#include "protocol/uri.h"

struct http_auth_basic {
	LIST_HEAD(struct http_auth_basic);

	unsigned char *url;
	unsigned char *realm;
	unsigned char *uid;
	unsigned char *passwd;
	unsigned int blocked:1;
	unsigned int valid:1;
};

#define auth_entry_has_userinfo(_entry_) \
	(*(_entry_)->uid && *(_entry_)->passwd)

unsigned char *find_auth(struct uri *);
struct http_auth_basic *add_auth_entry(struct uri *, unsigned char *);
void del_auth_entry(struct http_auth_basic *);
void free_auth(void);
struct http_auth_basic *get_invalid_auth_entry(void);

#endif
