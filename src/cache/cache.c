/* Cache subsystem */
/* $Id: cache.c,v 1.54 2003/10/16 08:28:19 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "config/options.h"
#include "document/cache.h"
#include "main.h"
#include "protocol/uri.h"
#include "sched/connection.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"
#include "util/types.h"

static INIT_LIST_HEAD(cache);
static long cache_size;
static int cache_count = 0;


/* Define to enable cache debugging features (redirect stderr to a file). */
/* #define DEBUG_CACHE */

#ifdef DEBUG_CACHE

#define dump_frag(frag, count) \
do { \
	debug(" [%d] f=%p offset=%li length=%li real_length=%li", \
	      count, frag, frag->offset, frag->length, frag->real_length); \
} while (0)

#define dump_frags(entry, comment) \
do { \
	struct fragment *frag; \
        int count = 0;	\
 \
	debug("url=%s, comment=%s cache_size=%li", entry->url, comment, cache_size); \
	foreach (frag, entry->frag) \
		dump_frag(frag, ++count); \
} while (0)

#else
#define dump_frags(entry, comment)
#endif /* DEBUG_CACHE */


static int
is_entry_used(struct cache_entry *e)
{
	struct connection *c;

	foreach (c, queue)
		if (c->cache == e)
			return 1;

	return 0;
}


long
cache_info(int type)
{
	int i = 0;
	struct cache_entry *ce;

	switch (type) {
		case INFO_BYTES:
			return cache_size;
		case INFO_FILES:
			foreach (ce, cache) i++;
			return i;
		case INFO_LOCKED:
			foreach (ce, cache) i += !!ce->refcount;
			return i;
		case INFO_LOADING:
			foreach (ce, cache) i += is_entry_used(ce);
			return i;
		case INFO_LIST:
			return (long) &cache;
		default:
			internal("cache_info: bad request");
	}
	return 0;
}

/* Return 1 and save cache entry to @f if there's matching one, otherwise
 * return 0. */
int
find_in_cache(unsigned char *url, struct cache_entry **f)
{
	struct cache_entry *e;

	url = extract_proxy(url);

	foreach (e, cache) {
		if (strcmp(e->url, url)) continue;

		/* Move it on the top of the list. */
		del_from_list(e);
		add_to_list(cache, e);

		*f = e;
		return 1;
	}

	return 0;
}

int
get_cache_entry(unsigned char *url, struct cache_entry **f)
{
	struct cache_entry *e;

	if (find_in_cache(url, f)) return 0;
	shrink_memory(0);
	url = extract_proxy(url);

	e = mem_calloc(1, sizeof(struct cache_entry));
	if (!e) return -1;

	e->url = stracpy(url);
	if (!e->url) {
		mem_free(e);
		return -1;
	}

	e->incomplete = 1;
	init_list(e->frag);
	e->count = cache_count++;

	add_to_list(cache, e);
	*f = e;

	return 0;
}

static inline void
resize_entry(struct cache_entry *e, int size)
{
	e->data_size += size;
	cache_size += size;
}

#define CACHE_PAD(x) (((x) | 0x3fff) + 1)

/* One byte is reserved for data in struct fragment. */
#define FRAGSIZE(x) (sizeof(struct fragment) + (x) - 1)

/* Add fragment to cache. Returns -1 upon error, 1 if cache entry was enlarged,
 * 0 if only old data were overwritten. Maybe. And maybe not. */
/* Note that this function is maybe overcommented, but I'm certainly not
 * unhappy from that. */
int
add_fragment(struct cache_entry *e, int offset,
	     unsigned char *data, int length)
{
	struct fragment *f, *nf;
	int ret = 0;
	int trunc = 0;
	int end_offset, f_end_offset;

	if (!length) return 0;

	end_offset = offset + length;
	if (e->length < end_offset)
		e->length = end_offset;

	/* XXX: This is probably some magic strange thing for HTML renderer. */
	e->count = cache_count++;

	/* Possibly insert the new data in the middle of existing fragment. */
	foreach (f, e->frag) {
		f_end_offset = f->offset + f->length;

		/* No intersection? */
		if (f->offset > offset) break;
		if (f_end_offset < offset) continue;

		if (end_offset > f_end_offset) {
			/* Overlap - we end further than original fragment. */

			ret = 1; /* !!! FIXME */

			/* Is intersected area same? Truncate it if not, dunno
			 * why though :). */
			if (memcmp(f->data + offset - f->offset, data,
				   f_end_offset - offset))
				trunc = 1;

			if (end_offset - f->offset <= f->real_length) {
				/* We fit here, so let's enlarge it by delta of
				 * old and new end.. */
				resize_entry(e, end_offset - f_end_offset);
				/* ..and length is now total length. */
				f->length = end_offset - f->offset;
			} else {
				/* We will reduce fragment length only to the
				 * starting non-interjecting size and add new
				 * fragment directly after this one. */
				f->length = offset - f->offset;
				f = f->next;
				break;
			}

		} else {
			/* We are subset of original fragment. */
			if (memcmp(f->data + offset - f->offset, data, length))
				trunc = 1;
		}

		/* Copy the stuff over there. */
		memcpy(f->data + offset - f->offset, data, length);
		goto remove_overlaps;
	}

	/* Make up new fragment. */
	/* One byte is reserved for data in struct fragment. */
	nf = mem_calloc(1, FRAGSIZE(CACHE_PAD(length)));
	if (!nf) return -1;

	nf->offset = offset;
	nf->length = length;
	nf->real_length = CACHE_PAD(length);
	memcpy(nf->data, data, length);
	add_at_pos(f->prev, nf);
	f = nf;

	ret = 1;
	resize_entry(e, length);

remove_overlaps:
	/* Contatenate overlapping fragments. */

	f_end_offset = f->offset + f->length;
	/* Iterate thru all fragments we still overlap to. */
	while ((void *) f->next != &e->frag
		&& f_end_offset > f->next->offset) {

		end_offset = f->next->offset + f->next->length;

		if (f_end_offset < end_offset) {
			/* We end before end of the following fragment, though.
			 * So try to append overlapping part of that fragment
			 * to us. */
			nf = mem_realloc(f, FRAGSIZE(end_offset - f->offset));
			if (!nf) goto ff;

			nf->prev->next = nf;
			nf->next->prev = nf;
			f = nf;

			if (memcmp(f->data + f->next->offset - f->offset,
				   f->next->data,
				   f->offset + f->length - f->next->offset))
				trunc = 1;

			memcpy(f->data + f->length,
			       f->next->data + f_end_offset - f->next->offset,
			       end_offset - f_end_offset);

			resize_entry(e, end_offset - f_end_offset);
			f->length = f->real_length = end_offset - f->offset;

ff:;
		} else {
			/* We will just discard this, it's complete subset of
			 * our new fragment. */
			if (memcmp(f->data + f->next->offset - f->offset,
				   f->next->data,
				   f->next->length))
				trunc = 1;
		}

		/* Remove the fragment, it influences our new one! */
		nf = f->next;
		resize_entry(e, -nf->length);
		del_from_list(nf);
		mem_free(nf);
	}

	if (trunc) truncate_entry(e, offset + length, 0);

	dump_frags(e, "add_fragment");

	return ret;
}

void
defrag_entry(struct cache_entry *e)
{
	struct fragment *f, *g, *h, *nf;
	int l;

	if (list_empty(e->frag)) return;
	f = e->frag.next;
	if (f->offset) return;

	for (g = f->next; g != (void *)&e->frag; g = g->next) {
		long overlay = g->prev->offset - g->prev->length - g->offset;

		if (overlay < 0) continue;
		if (overlay == 0) break;

		internal("fragments overlay");
		return;
	}

	if (g == f->next) return;

	for (l = 0, h = f; h != g; h = h->next)
		l += h->length;

	/* One byte is reserved for data in struct fragment. */
	nf = mem_calloc(1, FRAGSIZE(l));
	if (!nf) return;
	nf->length = l;
	nf->real_length = l;

	for (l = 0, h = f; h != g; h = h->next) {
		struct fragment *tmp = h;

		memcpy(nf->data + l, h->data, h->length);
		l += h->length;
		h = h->prev;
		del_from_list(tmp);
		mem_free(tmp);
	}
	add_to_list(e->frag, nf);

	dump_frags(e, "defrag_entry");
}

void
truncate_entry(struct cache_entry *e, int off, int final)
{
	struct fragment *f;

	if (e->length > off) {
		e->length = off;
		e->incomplete = 1;
	}

	foreach (f, e->frag) {
		long size = off - f->offset;

		if (size < 0) {

del:
			while ((void *)f != &e->frag) {
				struct fragment *tmp = f->next;

				resize_entry(e, -f->length);
				del_from_list(f);
				mem_free(f);
				f = tmp;
			}
			dump_frags(e, "truncate_entry");
			return;
		}

		if (f->length > size) {
			resize_entry(e, -(f->length - size));
			f->length = size;

			if (final) {
				struct fragment *nf;

				nf = mem_realloc(f, FRAGSIZE(f->length));
				if (nf) {
					nf->next->prev = nf;
					nf->prev->next = nf;
					f = nf;
					f->real_length = f->length;
				}
			}
			f = f->next;
			goto del;
		}
	}
}

void
free_entry_to(struct cache_entry *e, int off)
{
	struct fragment *f;

	foreach (f, e->frag) {
		if (f->offset + f->length <= off) {
			struct fragment *tmp = f;

			resize_entry(e, -f->length);
			f = f->prev;
			del_from_list(tmp);
			mem_free(tmp);
		} else if (f->offset < off) {
			long size = off - f->offset;

			resize_entry(e, -size);
			f->length -= size;
			memmove(f->data, f->data + size, f->length);
			f->offset = off;
		} else break;
	}
}

void
delete_entry_content(struct cache_entry *e)
{
	cache_size -= e->data_size;
	assertm(cache_size >= 0, "cache_size underflow: %ld", cache_size);
	if_assert_failed { cache_size = 0; }

	free_list(e->frag);
	e->count = cache_count++;
	e->length = 0;
	e->incomplete = 1;
	e->data_size = 0;

	if (e->last_modified) {
		mem_free(e->last_modified);
		e->last_modified = NULL;
	}

	if (e->etag) {
		mem_free(e->etag);
		e->etag = NULL;
	}
}

void
delete_cache_entry(struct cache_entry *e)
{
	assertm(!e->refcount, "deleting locked cache entry");
	assertm(!is_entry_used(e), "deleting loading cache entry");

	delete_entry_content(e);
	del_from_list(e);

	if (e->url) mem_free(e->url);
	if (e->head) mem_free(e->head);
	if (e->last_modified) mem_free(e->last_modified);
	if (e->redirect) mem_free(e->redirect);
	if (e->ssl_info) mem_free(e->ssl_info);
	if (e->encoding_info) mem_free(e->encoding_info);
	if (e->etag) mem_free(e->etag);

	mem_free(e);
}

void
garbage_collection(int whole)
{
	struct cache_entry *e, *f;
	long ncs = cache_size;
	long ccs = 0;
	int no = 0;
	long opt_cache_memory_size = get_opt_long("document.cache.memory.size");
	static long old_opt_cache_memory_size = -1;
	long opt_cache_gc_size = opt_cache_memory_size
				 * MEMORY_CACHE_GC_PERCENT / 100;

	if (old_opt_cache_memory_size != opt_cache_memory_size) {
		/* To force minimal cache size test in case of user changed
		 * option value. */
		whole = 1;
		old_opt_cache_memory_size = opt_cache_memory_size;
	}

#ifdef DEBUG_CACHE
	debug("gc %d", whole);
#endif
	if (!whole && cache_size <= opt_cache_memory_size) return;

	foreach (e, cache) {
		if (e->refcount || is_entry_used(e)) {
			ncs -= e->data_size;

			assertm(ncs >= 0, "cache_size underflow: %ld", ncs);
			if_assert_failed { ncs = 0; }
		}

		ccs += e->data_size;
	}

	assertm(ccs == cache_size, "cache_size badly computed: %ld != %ld",
		cache_size, ccs);
	if_assert_failed { cache_size = ccs; }

	if (!whole && ncs <= opt_cache_memory_size) return;

	foreachback (e, cache) {
		if (!whole && ncs <= opt_cache_gc_size)
			goto g;
		if (e->refcount || is_entry_used(e)) {
			no = 1;
			e->tgc = 0;
			continue;
		}
		e->tgc = 1;
		ncs -= e->data_size;

		assertm(ncs >= 0, "cache_size underflow: %ld", ncs);
		if_assert_failed { ncs = 0; }
	}

	assertm(ncs == 0, "cache_size overflow: %ld", ncs);
	if_assert_failed { ncs = 0; }

g:
	e = e->next;
	if ((void *) e == &cache) return;

	if (!whole) {
		for (f = e; (void *)f != &cache; f = f->next) {
			long newncs = ncs + f->data_size;

			if (newncs <= opt_cache_gc_size) {
				ncs = newncs;
				f->tgc = 0;
			}
		}
	}

	for (f = e; (void *)f != &cache;) {
		f = f->next;
		if (f->prev->tgc)
			delete_cache_entry(f->prev);
	}

	if ((whole || !no) && cache_size > opt_cache_gc_size) {
		/* Given cache size is too low regarding currently used cache
		 * elements so we set it to a reasonable value. */
		/* TODO: warn user about it. */
		get_opt_long("document.cache.memory.size") =
			CACHE_PAD(cache_size * 100 / MEMORY_CACHE_GC_PERCENT);
#ifdef DEBUG_CACHE
		debug("garbage collection doesn't work, cache size %li > %li, "
		      "document.cache.memory.size set to: %li bytes",
		      cache_size, opt_cache_gc_size,
		      get_opt_long("document.cache.memory.size"));
#endif
	}
}
