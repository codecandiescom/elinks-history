/* $Id: cache.h,v 1.26 2003/11/08 01:06:32 pasky Exp $ */

#ifndef EL__CACHE_CACHE_H
#define EL__CACHE_CACHE_H

#include "util/lists.h"

enum cache_mode {
	NC_ALWAYS_CACHE,
	NC_CACHE,
	NC_IF_MOD,
	NC_RELOAD,
	NC_PR_NO_CACHE,
};

struct cache_entry {
	LIST_HEAD(struct cache_entry);

	struct list_head frag;

	unsigned char *url;
	unsigned char *head;
	unsigned char *redirect;
	unsigned char *last_modified;
	unsigned char *etag;
	unsigned char *ssl_info;
	unsigned char *encoding_info;

	unsigned int id_tag; /* Change each time entry is modified. */

	int length;
	int data_size;
	int refcount;

#ifdef HAVE_SCRIPTING
	unsigned int done_pre_format_html_hook:1;
#endif
	unsigned int redirect_get:1;
	unsigned int incomplete:1;

	/* This is a mark for internal workings of garbage_collector(), whether
	 * the cache_entry should be busted or not. You are not likely to see
	 * an entry with this set to 1 in wild nature ;-). */
	unsigned int gc_target:1;

	enum cache_mode cache_mode;
};

struct fragment {
	LIST_HEAD(struct fragment);

	int offset;
	int length;
	int real_length;
	unsigned char data[1]; /* Must be last */
};

long cache_info(int);
int find_in_cache(unsigned char *, struct cache_entry **);
int get_cache_entry(unsigned char *, struct cache_entry **);
int add_fragment(struct cache_entry *, int, unsigned char *, int);
void defrag_entry(struct cache_entry *);
void truncate_entry(struct cache_entry *, int, int);
void free_entry_to(struct cache_entry *, int);
void delete_entry_content(struct cache_entry *);
void delete_cache_entry(struct cache_entry *);
void garbage_collection(int);

#endif
