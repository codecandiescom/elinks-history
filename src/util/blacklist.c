/* Blacklist manager */
/* $Id: blacklist.c,v 1.10 2003/07/07 15:36:04 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "util/blacklist.h"
#include "util/lists.h"
#include "util/memory.h"


struct blacklist_entry {
	LIST_HEAD(struct blacklist_entry);

	enum blacklist_flags flags;
	unsigned char host[1];
};

INIT_LIST_HEAD(blacklist);

void
add_blacklist_entry(unsigned char *host, int hostlen, enum blacklist_flags flags)
{
	struct blacklist_entry *b;

	foreach (b, blacklist) {
		if (strncasecmp(host, b->host, hostlen)) continue;

		b->flags |= flags;
		return;
	}

	b = mem_alloc(sizeof(struct blacklist_entry) + hostlen);
	if (!b) return;

	b->flags = flags;
	memcpy(b->host, host, hostlen);
	add_to_list(blacklist, b);
}

void
del_blacklist_entry(unsigned char *host, int hostlen, enum blacklist_flags flags)
{
	struct blacklist_entry *b;

	foreach (b, blacklist) {
		if (strncasecmp(host, b->host, hostlen)) continue;

		b->flags &= ~flags;
		if (!b->flags) {
			del_from_list(b);
			mem_free(b);
		}
		return;
	}
}

int
get_blacklist_flags(unsigned char *host, int hostlen)
{
	struct blacklist_entry *b;

	foreach (b, blacklist)
		if (!strncasecmp(host, b->host, hostlen))
			return b->flags;
	return 0;
}

void
free_blacklist(void)
{
	free_list(blacklist);
}
