/* $Id: blacklist.h,v 1.4 2004/03/21 01:37:54 jonas Exp $ */

#ifndef EL__UTIL_BLACKLIST_H
#define EL__UTIL_BLACKLIST_H

enum blacklist_flags {
	BL_HTTP10 = 1,
	BL_NO_CHARSET = 2,
};

struct string;

void add_blacklist_entry(struct string *host, enum blacklist_flags);
void del_blacklist_entry(struct string *host, enum blacklist_flags);
int get_blacklist_flags(struct string *host);
void free_blacklist(void);

#endif
