/* Blacklist manager */
/* $Id: blacklist.c,v 1.14 2003/07/25 12:32:22 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "util/blacklist.h"
#include "util/lists.h"
#include "util/memory.h"
#include "util/string.h"


struct blacklist_entry {
	LIST_HEAD(struct blacklist_entry);

	enum blacklist_flags flags;
	unsigned char host[1]; /* Must be last. */
};

INIT_LIST_HEAD(blacklist);

void
add_blacklist_entry(unsigned char *host, int hostlen, enum blacklist_flags flags)
{
	struct blacklist_entry *b;

	assert(host && hostlen > 0);
	if_assert_failed return;

	foreach (b, blacklist) {
		if (strncasecmp(b->host, host, hostlen)) continue;

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

	assert(host && hostlen > 0);
	if_assert_failed return;

	foreach (b, blacklist) {
		if (strncasecmp(b->host, host, hostlen)) continue;

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

	assert(host && hostlen > 0);
	if_assert_failed return 0;

	foreach (b, blacklist)
		if (!strncasecmp(b->host, host, hostlen))
			return b->flags;
	return 0;
}

void
free_blacklist(void)
{
	free_list(blacklist);
}
