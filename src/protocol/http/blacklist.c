/* Blacklist manager */
/* $Id: blacklist.c,v 1.3 2002/06/17 07:42:32 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "links.h"

#include "util/blacklist.h"
#include "util/memory.h"


struct blacklist_entry {
	struct blacklist_entry *next;
	struct blacklist_entry *prev;
	enum blacklist_flags flags;
	unsigned char host[1];
};

struct list_head blacklist = { &blacklist, &blacklist };


/* add_blacklist_entry() */
void
add_blacklist_entry(unsigned char *host, enum blacklist_flags flags)
{
	struct blacklist_entry *b;

	foreach(b, blacklist) {
		if (strcasecmp(host, b->host)) continue;

		b->flags |= flags;
		return;
	}

	b = mem_alloc(sizeof(struct blacklist_entry) + strlen(host) + 1);
	if (!b) return;

	b->flags = flags;
	strcpy(b->host, host);
	add_to_list(blacklist, b);
}


/* del_blacklist_entry() */
void
del_blacklist_entry(unsigned char *host, enum blacklist_flags flags)
{
	struct blacklist_entry *b;

	foreach(b, blacklist) {
		if (strcasecmp(host, b->host)) continue;

		b->flags &= ~flags;
		if (!b->flags) {
			del_from_list(b);
			mem_free(b);
		}
		return;
	}
}


/* get_blacklist_flags() */
int
get_blacklist_flags(unsigned char *host)
{
	struct blacklist_entry *b;

	foreach(b, blacklist)
		if (!strcasecmp(host, b->host))
			return b->flags;
	return 0;
}


/* free_blacklist() */
void
free_blacklist()
{
	free_list(blacklist);
}
