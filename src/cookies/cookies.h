/* $Id: cookies.h,v 1.6 2003/07/08 01:24:26 jonas Exp $ */

#ifndef EL__COOKIES_COOKIES_H
#define EL__COOLIES_COOKIES_H

#include "protocol/uri.h"
#include "terminal/terminal.h"

int set_cookie(struct terminal *, unsigned char *, unsigned char *);
void send_cookies(unsigned char **, int *, struct uri *uri);
void load_cookies(void);
void init_cookies(void);
void cleanup_cookies(void);

#endif
