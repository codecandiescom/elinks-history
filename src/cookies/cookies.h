/* $Id: cookies.h,v 1.22 2004/05/31 02:22:37 jonas Exp $ */

#ifndef EL__COOKIES_COOKIES_H
#define EL__COOKIES_COOKIES_H

#include "bfu/listbox.h"
#include "modules/module.h"
#include "protocol/uri.h"
#include "util/object.h"
#include "util/string.h"
#include "util/ttime.h"

enum cookies_accept {
	COOKIES_ACCEPT_NONE,
	COOKIES_ACCEPT_ASK,
	COOKIES_ACCEPT_ALL
};

struct c_server {
	LIST_HEAD(struct c_server);

	struct listbox_item *box_item;
	struct object object;
	int accept;
	unsigned char server[1]; /* Must be at end of struct. */
};

struct cookie {
	LIST_HEAD(struct cookie);

	unsigned char *name, *value;
	struct c_server *server;
	unsigned char *path, *domain;
	ttime expires; /* zero means undefined */
	int secure;
	int id;

	/* This is indeed maintained by cookies.c, not dialogs.c; much easier
	 * and simpler. */
	struct listbox_item *box_item;
	struct object object;
};

struct c_server *get_cookie_server(unsigned char *host, int hostlen);
void accept_cookie(struct cookie *);
void free_cookie(struct cookie *);
void set_cookie(struct uri *, unsigned char *);
void load_cookies(void);
void save_cookies(void);

/* Note that the returned value points to a static structure and thus the
 * string will be overwritten at the next call time. */
struct string *send_cookies(struct uri *uri);

extern struct module cookies_module;

#endif
