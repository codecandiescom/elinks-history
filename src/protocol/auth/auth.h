/* $Id: auth.h,v 1.21 2004/11/14 10:32:55 witekfl Exp $ */

#ifndef EL__PROTOCOL_AUTH_AUTH_H
#define EL__PROTOCOL_AUTH_AUTH_H

#include "protocol/uri.h"
#include "util/object.h"

struct listbox_item;

struct http_auth_basic {
	LIST_HEAD(struct http_auth_basic);

	struct uri *uri;
	unsigned char *realm;
	unsigned char *nonce;
	unsigned char *opaque;

	struct listbox_item *box_item;
	struct object object;

	unsigned char user[HTTP_AUTH_USER_MAXLEN];
	unsigned char password[HTTP_AUTH_PASSWORD_MAXLEN];
	unsigned int blocked:1;
	unsigned int valid:1;
	unsigned int digest:1;
};

#define auth_entry_has_userinfo(_entry_) \
	(*(_entry_)->user || *(_entry_)->password)

unsigned char *find_auth(struct uri *);
struct http_auth_basic *find_auth_entry(struct uri *uri, unsigned char *realm);
struct http_auth_basic *add_auth_entry(struct uri *, unsigned char *);
void del_auth_entry(struct http_auth_basic *);
void free_auth(void);
struct http_auth_basic *get_invalid_auth_entry(void);

#endif
