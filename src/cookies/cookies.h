/* $Id: cookies.h,v 1.13 2003/11/17 22:10:58 jonas Exp $ */

#ifndef EL__COOKIES_COOKIES_H
#define EL__COOKIES_COOKIES_H

#include "bfu/listbox.h"
#include "lowlevel/ttime.h"
#include "modules/module.h"
#include "protocol/uri.h"
#include "util/string.h"

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
	int refcount;
};

void set_cookie(struct uri *, unsigned char *);
void send_cookies(struct string *header, struct uri *uri);
void load_cookies(void);

extern struct module cookies_module;
extern struct list_head cookie_box_items; /* struct listbox_item */
extern struct list_head cookie_boxes; /* struct listbox_data */


#endif
