/* $Id: blacklist.h,v 1.1 2002/04/21 19:12:35 pasky Exp $ */

#ifndef EL__UTIL_BLACKLIST_H
#define EL__UTIL_BLACKLIST_H

enum blacklist_flags {
	BL_HTTP10 = 1,
	BL_NO_CHARSET = 2,
};

void add_blacklist_entry(unsigned char *, enum blacklist_flags);
void del_blacklist_entry(unsigned char *, enum blacklist_flags);
int get_blacklist_flags(unsigned char *);
void free_blacklist();

#endif
