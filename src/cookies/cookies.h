/* $Id: cookies.h,v 1.12 2003/11/16 01:02:47 jonas Exp $ */

#ifndef EL__COOKIES_COOKIES_H
#define EL__COOKIES_COOKIES_H

#include "modules/module.h"
#include "protocol/uri.h"
#include "util/string.h"

enum cookies_accept {
	COOKIES_ACCEPT_NONE,
	COOKIES_ACCEPT_ASK,
	COOKIES_ACCEPT_ALL
};

void set_cookie(struct uri *, unsigned char *);
void send_cookies(struct string *header, struct uri *uri);
void load_cookies(void);

extern struct module cookies_module;

#endif
