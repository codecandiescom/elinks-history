/* $Id: cookies.h,v 1.8 2003/07/21 04:59:27 jonas Exp $ */

#ifndef EL__COOKIES_COOKIES_H
#define EL__COOLIES_COOKIES_H

#include "protocol/uri.h"
#include "terminal/terminal.h"
#include "util/string.h"

int set_cookie(struct terminal *, struct uri *, unsigned char *);
void send_cookies(struct string *header, struct uri *uri);
void load_cookies(void);
void init_cookies(void);
void cleanup_cookies(void);

#endif
