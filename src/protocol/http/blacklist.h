/* $Id: blacklist.h,v 1.7 2004/07/15 16:01:51 jonas Exp $ */

#ifndef EL__PROTOCOL_HTTP_BLACKLIST_H
#define EL__PROTOCOL_HTTP_BLACKLIST_H

struct uri;

enum blacklist_flags {
	SERVER_BLACKLIST_HTTP10 = 1,
	SERVER_BLACKLIST_NO_CHARSET = 2,
};

void add_blacklist_entry(struct uri *, enum blacklist_flags);
void del_blacklist_entry(struct uri *, enum blacklist_flags);
int get_blacklist_flags(struct uri *);
void free_blacklist(void);

#endif
