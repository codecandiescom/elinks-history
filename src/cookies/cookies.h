/* $Id: cookies.h,v 1.9 2003/10/26 14:58:55 jonas Exp $ */

#ifndef EL__COOKIES_COOKIES_H
#define EL__COOLIES_COOKIES_H

#include "modules/module.h"
#include "protocol/uri.h"
#include "terminal/terminal.h"
#include "util/string.h"

int set_cookie(struct terminal *, struct uri *, unsigned char *);
void send_cookies(struct string *header, struct uri *uri);
void load_cookies(void);

extern struct module cookies_module;

#endif
