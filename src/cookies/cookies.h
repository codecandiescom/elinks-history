/* $Id: cookies.h,v 1.5 2003/05/08 21:50:07 zas Exp $ */

#ifndef EL__COOKIES_H
#define EL__COOKIES_H

#include "terminal/terminal.h"

int set_cookie(struct terminal *, unsigned char *, unsigned char *);
void send_cookies(unsigned char **, int *, unsigned char *);
void load_cookies(void);
void init_cookies(void);
void cleanup_cookies(void);

#endif
