/* $Id: cookies.h,v 1.4 2003/05/04 17:25:52 pasky Exp $ */

#ifndef EL__COOKIES_H
#define EL__COOKIES_H

#include "terminal/terminal.h"

int set_cookie(struct terminal *, unsigned char *, unsigned char *);
void send_cookies(unsigned char **, int *, unsigned char *);
void load_cookies();
void init_cookies();
void cleanup_cookies();

#endif
