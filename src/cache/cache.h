/* $Id: cache.h,v 1.13 2003/05/07 09:49:01 zas Exp $ */

#ifndef EL__CACHE_H
#define EL__CACHE_H

#include "util/lists.h"
#include "elinks.h" /* tcount */

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
	unsigned char *encoding_info;
	unsigned char *last_modified;
	unsigned char *etag;
	unsigned char *ssl_info;
	
	tcount count;

	int redirect_get;
	int length;
	int incomplete;
	int tgc;
	int data_size;
	int refcount;
#ifdef HAVE_SCRIPTING
	int done_pre_format_html_hook;
#endif

	enum cache_mode cache_mode;
	
};

struct fragment {
	LIST_HEAD(struct fragment);

	int offset;
	int length;
	int real_length;
	unsigned char data[1]; /* must be at end of struct */
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
