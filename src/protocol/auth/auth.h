/* $Id: auth.h,v 1.10 2003/07/10 13:19:38 jonas Exp $ */

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

enum add_auth_code {
	ADD_AUTH_ERROR	= -1,
	ADD_AUTH_NONE	= 0,
	ADD_AUTH_EXIST	= 1,
	ADD_AUTH_NEW	= 2,
};

unsigned char *find_auth(struct uri *);
enum add_auth_code add_auth_entry(struct uri *, unsigned char *);
void del_auth_entry(struct http_auth_basic *);
void free_auth(void);

#endif
