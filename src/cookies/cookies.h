/* $Id: cookies.h,v 1.7 2003/07/08 12:39:31 jonas Exp $ */

#ifndef EL__COOKIES_COOKIES_H
#define EL__COOLIES_COOKIES_H

#include "protocol/uri.h"
#include "terminal/terminal.h"

int set_cookie(struct terminal *, struct uri *, unsigned char *);
void send_cookies(unsigned char **, int *, struct uri *uri);
void load_cookies(void);
void init_cookies(void);
void cleanup_cookies(void);

#endif
