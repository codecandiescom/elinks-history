/* $Id: cookies.h,v 1.21 2004/04/03 17:40:53 jonas Exp $ */

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

struct cookie {
	LIST_HEAD(struct cookie);

	unsigned char *name, *value;
	unsigned char *server;
	unsigned char *path, *domain;
	ttime expires; /* zero means undefined */
	int secure;
	int id;

	/* This is indeed maintained by cookies.c, not dialogs.c; much easier
	 * and simpler. */
	struct listbox_item *box_item;
	struct object object;
};

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
