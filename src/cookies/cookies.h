/* $Id: cookies.h,v 1.10 2003/10/26 19:18:17 jonas Exp $ */

#ifndef EL__COOKIES_COOKIES_H
#define EL__COOLIES_COOKIES_H

#include "modules/module.h"
#include "protocol/uri.h"
#include "terminal/terminal.h"
#include "util/string.h"

enum cookies_accept {
	COOKIES_ACCEPT_NONE,
	COOKIES_ACCEPT_ASK,
	COOKIES_ACCEPT_ALL
};

int set_cookie(struct terminal *, struct uri *, unsigned char *);
void send_cookies(struct string *header, struct uri *uri);
void load_cookies(void);

extern struct module cookies_module;

#endif
