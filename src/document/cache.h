/* $Id: cache.h,v 1.17 2003/07/02 00:16:03 jonas Exp $ */

#ifndef EL__DOCUMENT_CACHE_H
#define EL__DOCUMENT_CACHE_H

#include "elinks.h" /* tcount */

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

	unsigned char *url;
	unsigned char *head;
	unsigned char *redirect;
	int redirect_get;
	int length;
	int incomplete;
	int tgc;
	enum cache_mode cache_mode;
	unsigned char *last_modified;
	unsigned char *etag;
	int data_size;
	struct list_head frag; /* i don't know why yet, but do not move --Zas */
	tcount count;
	int refcount;
	unsigned char *ssl_info;
	unsigned char *encoding_info;
#ifdef HAVE_SCRIPTING
	int done_pre_format_html_hook;
#endif
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
