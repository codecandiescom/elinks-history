/* $Id: blacklist.h,v 1.5 2004/03/22 14:35:40 jonas Exp $ */

#ifndef EL__UTIL_BLACKLIST_H
#define EL__UTIL_BLACKLIST_H

enum blacklist_flags {
	BL_HTTP10 = 1,
	BL_NO_CHARSET = 2,
};

void add_blacklist_entry(unsigned char *, int, enum blacklist_flags);
void del_blacklist_entry(unsigned char *, int, enum blacklist_flags);
int get_blacklist_flags(unsigned char *, int);
void free_blacklist(void);

#endif
