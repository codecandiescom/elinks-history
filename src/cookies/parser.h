/* $Id: parser.h,v 1.1 2002/04/23 08:06:21 pasky Exp $ */

#ifndef EL__COOKIES_PARSER_H
#define EL__COOKIES_PARSER_H

struct cookie_str {
	unsigned char *str;
	unsigned char *nam_end, *val_start, *val_end;
};

struct cookie_str *parse_cookie_str(struct cookie_str *);

#endif
