/* $Id: cache.h,v 1.85 2004/05/25 03:40:18 jonas Exp $ */

#ifndef EL__CACHE_CACHE_H
#define EL__CACHE_CACHE_H

#include "util/lists.h"
#include "util/object.h"

struct listbox_item;
struct uri;

/* This enum describes the level of caching of certain cache entry. That is,
 * under what conditions shall it be reloaded, if ever. The one with lowest
 * value is most agressively cached, however the cache is most reluctant to
 * cache the one with highest value. */
/* TODO: Invert order to be more intuitive. But be careful, many tests rely on
 * current order. --Miciah,Zas */
enum cache_mode {
	CACHE_MODE_INCREMENT = -1,
	CACHE_MODE_ALWAYS,
	CACHE_MODE_NORMAL,
	CACHE_MODE_CHECK_IF_MODIFIED,
	CACHE_MODE_FORCE_RELOAD,
	CACHE_MODE_NEVER,
};

struct cache_entry {
	LIST_HEAD(struct cache_entry);

	struct list_head frag;		/* -> struct fragment */

	struct uri *uri;		/* Identifier for the cached data */
	struct uri *proxy_uri;		/* Proxy identifier or same as @uri */
	struct uri *redirect;		/* Location we were redirected to */

	unsigned char *head;		/* The protocol header */
	unsigned char *last_modified;	/* Latest modification date */
	unsigned char *etag;		/* ETag value from the HTTP header */
	unsigned char *ssl_info;	/* SSL ciphers used during transfer */
	unsigned char *encoding_info;	/* Encoding used during transfer */

	unsigned int id_tag;		/* Change each time entry is modified. */

	int length;			/* The expected and complete size */
	int data_size;			/* The actual size of all fragments */

	struct listbox_item *box_item;	/* Dialog data for cache manager */
	struct object object;		/* Usage refcount object */

	unsigned int preformatted:1;	/* Has content been preformatted? */
	unsigned int redirect_get:1;	/* Follow redirect using get method? */
	unsigned int incomplete:1;	/* Has all data been downloaded? */
	unsigned int valid:1;		/* Is cache entry usable? */

	/* This is a mark for internal workings of garbage_collection(), whether
	 * the cache_entry should be busted or not. You are not likely to see
	 * an entry with this set to 1 in wild nature ;-). */
	unsigned int gc_target:1;	/* The GC touch of death */

	enum cache_mode cache_mode;	/* Reload condition */
};

/* Cache entries lists */

#define get_cache_uri(cache_entry) \
	((cache_entry)->valid ? (cache_entry)->uri : NULL)

struct fragment {
	LIST_HEAD(struct fragment);

	int offset;
	int length;
	int real_length;
	unsigned char data[1]; /* Must be last */
};

long cache_info(int);

/* Searches the cache for an entry matching the URI. Returns NULL if no one
 * matches. */
struct cache_entry *find_in_cache(struct uri *uri);

/* Searches the cache for a matching entry else a new one is added. Returns
 * NULL if allocation fails. */
struct cache_entry *get_cache_entry(struct uri *uri);

int add_fragment(struct cache_entry *, int, unsigned char *, int);
void defrag_entry(struct cache_entry *);
void truncate_entry(struct cache_entry *, int, int);
void free_entry_to(struct cache_entry *, int);
void delete_entry_content(struct cache_entry *);
void delete_cache_entry(struct cache_entry *);

/* Sets up the cache entry to redirect to a new location
 * @location	decides where to redirect to by resolving it relative to the
 *		entry's URI.
 * @get		controls the method should be used when handling the redirect.
 * @incomplete	will become the new value of the incomplete member if it
 *		is >= 0.
 * Returns the URI being redirected to or NULL if allocation failed.
 */
struct uri *
redirect_cache(struct cache_entry *cached, unsigned char *location,
		int get, int incomplete);

/* The garbage collector trigger. If @whole is zero, remove unused cache
 * entries which are bigger than the cache size limit set by user. For @zero
 * being one, remove all unused cache entries. */
void garbage_collection(int whole);

#endif
