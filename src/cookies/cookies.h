/* $Id: cookies.h,v 1.2 2002/03/17 13:54:12 pasky Exp $ */

#ifndef EL__COOKIES_H
#define EL__COOKIES_H

#include <lowlevel/terminal.h>

int set_cookie(struct terminal *, unsigned char *, unsigned char *);
void send_cookies(unsigned char **, int *, unsigned char *);
void load_cookies();
void init_cookies();
void cleanup_cookies();

#endif
