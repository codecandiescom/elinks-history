/* Blacklist manager */
/* $Id: blacklist.c,v 1.23 2004/07/15 16:18:30 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "protocol/http/blacklist.h"
#include "protocol/uri.h"
#include "util/lists.h"
#include "util/memory.h"
#include "util/string.h"


struct blacklist_entry {
	LIST_HEAD(struct blacklist_entry);

	enum blacklist_flags flags;
	unsigned char host[1]; /* Must be last. */
};

static INIT_LIST_HEAD(blacklist);


static struct blacklist_entry *
get_blacklist_entry(struct uri *uri)
{
	struct blacklist_entry *entry;

	assert(uri && uri->hostlen > 0);
	if_assert_failed return 0;

	foreach (entry, blacklist)
		if (!strncasecmp(entry->host, uri->host, uri->hostlen))
			return entry;

	return NULL;
}

void
add_blacklist_entry(struct uri *uri, enum blacklist_flags flags)
{
	struct blacklist_entry *b;

	assert(uri && uri->hostlen > 0);
	if_assert_failed return;

	foreach (b, blacklist) {
		if (strncasecmp(b->host, uri->host, uri->hostlen))
			continue;

		b->flags |= flags;
		return;
	}

	b = mem_alloc(sizeof(struct blacklist_entry) + uri->hostlen);
	if (!b) return;

	b->flags = flags;
	memcpy(b->host, uri->host, uri->hostlen);
	add_to_list(blacklist, b);
}

void
del_blacklist_entry(struct uri *uri, enum blacklist_flags flags)
{
	struct blacklist_entry *entry = get_blacklist_entry(uri);

	if (!entry) return;

	entry->flags &= ~flags;
	if (entry->flags) return;

	del_from_list(entry);
	mem_free(entry);
}

enum blacklist_flags
get_blacklist_flags(struct uri *uri)
{
	struct blacklist_entry *entry = get_blacklist_entry(uri);

	return entry ? entry->flags : SERVER_BLACKLIST_NONE;
}

void
free_blacklist(void)
{
	free_list(blacklist);
}
