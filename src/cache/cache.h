/* $Id: cache.h,v 1.57 2003/11/17 17:58:57 pasky Exp $ */

#ifndef EL__CACHE_CACHE_H
#define EL__CACHE_CACHE_H

/* #include "bfu/listbox.h" */
struct listbox_item;

#include "protocol/uri.h"
#include "util/lists.h"

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

	/* This is indeed maintained by cache.c, not dialogs.c; much easier
	 * and simpler. */
	struct listbox_item *box_item;

	struct list_head frag;

	struct uri uri;
	unsigned char *head;
	unsigned char *redirect;
	unsigned char *last_modified;
	unsigned char *etag;
	unsigned char *ssl_info;
	unsigned char *encoding_info;

	unsigned int id_tag; /* Change each time entry is modified. */

	int length;
	int data_size;
	int refcount; /* No direct access, use provided macros for that. */

#ifdef HAVE_SCRIPTING
	unsigned int done_pre_format_html_hook:1;
#endif
	unsigned int redirect_get:1;
	unsigned int incomplete:1;
	unsigned int valid:1;

	/* This is a mark for internal workings of garbage_collection(), whether
	 * the cache_entry should be busted or not. You are not likely to see
	 * an entry with this set to 1 in wild nature ;-). */
	unsigned int gc_target:1;

	enum cache_mode cache_mode;
};

/* Cache entries lists */

extern struct list_head cache_entry_box_items; /* struct listbox_item */
extern struct list_head cache_entry_boxes; /* struct listbox_data */


#if 0
#define DEBUG_CACHE_ENTRIES_LOCKS
#endif

#ifdef DEBUG_CACHE_ENTRIES_LOCKS
#define ce_lock_debug(ce, info) debug("cache entry %p lock %s now %d url= %s", ce, info, (ce)->refcount, struri((ce)->uri))
#else
#define ce_lock_debug(ce, info)
#endif

#ifdef DEBUG
#include "util/error.h"
#define ce_sanity_check(ce) \
do { \
	assert(ce); \
	assertm((ce)->refcount >= 0, "Cache entry refcount underflow."); \
	if_assert_failed (ce)->refcount = 0; \
} while (0)
#else
#define ce_sanity_check(ce)
#endif

#define get_cache_entry_refcount(ce) ((ce)->refcount)
#define is_cache_entry_used(ce) (!!(ce)->refcount)
#define cache_entry_lock(ce) do { ce_sanity_check(ce); (ce)->refcount++; ce_lock_debug(ce, "+1"); } while (0)
#define cache_entry_unlock(ce) do { (ce)->refcount--; ce_lock_debug(ce, "-1"); ce_sanity_check(ce);} while (0)

/* Please keep this one. It serves for debugging. --Zas */
#define cache_entry_nolock(ce) do { ce_sanity_check(ce); ce_lock_debug(ce, "0"); } while (0)

#define get_cache_uri(cache_entry) \
	((cache_entry)->valid ? struri((cache_entry)->uri) : (unsigned char *) "")

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
struct cache_entry *find_in_cache(unsigned char *uri);

/* Searches the cache for a matching entry else a new one is added. Returns
 * NULL if allocation fails. */
struct cache_entry *get_cache_entry(unsigned char *uri);

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
