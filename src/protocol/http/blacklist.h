/* $Id: blacklist.h,v 1.6 2004/07/15 15:52:32 jonas Exp $ */

#ifndef EL__PROTOCOL_HTTP_BLACKLIST_H
#define EL__PROTOCOL_HTTP_BLACKLIST_H

enum blacklist_flags {
	SERVER_BLACKLIST_HTTP10 = 1,
	SERVER_BLACKLIST_NO_CHARSET = 2,
};

void add_blacklist_entry(unsigned char *, int, enum blacklist_flags);
void del_blacklist_entry(unsigned char *, int, enum blacklist_flags);
int get_blacklist_flags(unsigned char *, int);
void free_blacklist(void);

#endif
