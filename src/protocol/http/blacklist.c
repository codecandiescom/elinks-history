/* Blacklist manager */
/* $Id: blacklist.c,v 1.16 2004/03/21 01:37:54 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "protocol/http/blacklist.h"
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
add_blacklist_entry(struct string *host, enum blacklist_flags flags)
{
	struct blacklist_entry *b;

	assert(!string_is_empty(host));
	if_assert_failed return;

	foreach (b, blacklist) {
		if (string_strcasecmp(host, b->host)) continue;

		b->flags |= flags;
		return;
	}

	b = mem_alloc(sizeof(struct blacklist_entry) + host->length);
	if (!b) return;

	b->flags = flags;
	string_copy(b->host, host);
	add_to_list(blacklist, b);
}

void
del_blacklist_entry(struct string *host, enum blacklist_flags flags)
{
	struct blacklist_entry *b;

	assert(!string_is_empty(host));
	if_assert_failed return;

	foreach (b, blacklist) {
		if (string_strcasecmp(host, b->host)) continue;

		b->flags &= ~flags;
		if (!b->flags) {
			del_from_list(b);
			mem_free(b);
		}
		return;
	}
}

int
get_blacklist_flags(struct string *host)
{
	struct blacklist_entry *b;

	assert(!string_is_empty(host));
	if_assert_failed return 0;

	foreach (b, blacklist)
		if (!string_strcasecmp(host, b->host))
			return b->flags;
	return 0;
}

void
free_blacklist(void)
{
	free_list(blacklist);
}
