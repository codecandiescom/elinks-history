/* $Id: cache.h,v 1.29 2003/11/08 12:47:01 pasky Exp $ */

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
	int locks;

#ifdef HAVE_SCRIPTING
	unsigned int done_pre_format_html_hook:1;
#endif
	unsigned int redirect_get:1;
	unsigned int incomplete:1;

	/* This is a mark for internal workings of garbage_collection(), whether
	 * the cache_entry should be busted or not. You are not likely to see
	 * an entry with this set to 1 in wild nature ;-). */
	unsigned int gc_target:1;

	enum cache_mode cache_mode;
};

#define get_cache_entry_locks(ce) ((ce)->locks)
#define is_cache_entry_locked(ce) (!!(ce)->locks)
#define cache_entry_lock_inc(ce) do { (ce)->locks++; } while (0)
#define cache_entry_lock_dec(ce) do { (ce)->locks--; } while (0)

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

/* The garbage collector trigger. If @whole is zero, remove unused cache
 * entries which are bigger than the cache size limit set by user. For @zero
 * being one, remove all unused cache entries. */
void garbage_collection(int whole);

#endif
