/* $Id: cookies.h,v 1.3 2002/03/16 22:03:09 pasky Exp $ */

#ifndef EL__COOKIES_H
#define EL__COOKIES_H

#include "terminal.h"

int set_cookie(struct terminal *, unsigned char *, unsigned char *);
void send_cookies(unsigned char **, int *, unsigned char *);
void load_cookies();
void init_cookies();
void cleanup_cookies();

#endif
