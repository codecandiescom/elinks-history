/* $Id: cookies.h,v 1.3 2002/05/08 13:55:01 pasky Exp $ */

#ifndef EL__COOKIES_H
#define EL__COOKIES_H

#include "lowlevel/terminal.h"

int set_cookie(struct terminal *, unsigned char *, unsigned char *);
void send_cookies(unsigned char **, int *, unsigned char *);
void load_cookies();
void init_cookies();
void cleanup_cookies();

#endif
