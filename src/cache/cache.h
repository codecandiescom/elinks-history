/* $Id: cache.h,v 1.7 2002/08/27 13:31:23 pasky Exp $ */

#ifndef EL__CACHE_H
#define EL__CACHE_H

#include "util/lists.h"
#include "links.h" /* tcount */

struct cache_entry {
	struct cache_entry *next;
	struct cache_entry *prev;
	unsigned char *url;
	unsigned char *head;
	unsigned char *redirect;
	int redirect_get;
	int length;
	int incomplete;
	int tgc;
	unsigned char *last_modified;
	int data_size;
	struct list_head frag;
	tcount count;
	int refcount;
	unsigned char *ssl_info;
	unsigned char *encoding_info;
#ifdef HAVE_SCRIPTING
	int done_pre_format_html_hook;
#endif
};

struct fragment {
	struct fragment *next;
	struct fragment *prev;
	int offset;
	int length;
	int real_length;
	unsigned char data[1];
};

long cache_info(int);
int find_in_cache(unsigned char *, struct cache_entry **);
int get_cache_entry(unsigned char *, struct cache_entry **);
/* int get_cache_data(struct cache_entry *e, unsigned char **, int *); */
int add_fragment(struct cache_entry *, int, unsigned char *, int);
void defrag_entry(struct cache_entry *);
void truncate_entry(struct cache_entry *, int, int);
void free_entry_to(struct cache_entry *, int);
void delete_entry_content(struct cache_entry *);
void garbage_collection(int);

#endif
