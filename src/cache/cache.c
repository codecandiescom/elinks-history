/* Cache subsystem */
/* $Id: cache.c,v 1.28 2003/01/07 19:57:29 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "main.h"
#include "config/options.h"
#include "document/cache.h"
#include "protocol/url.h"
#include "sched/sched.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"

static struct list_head cache = {&cache, &cache};

static long cache_size;

static int cache_count = 0;


static int
is_entry_used(struct cache_entry *e)
{
	struct connection *c;

	foreach(c, queue)
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
		case CI_BYTES:
			return cache_size;
		case CI_FILES:
			foreach(ce, cache) i++;
			return i;
		case CI_LOCKED:
			foreach(ce, cache) i += !!ce->refcount;
			return i;
		case CI_LOADING:
			foreach(ce, cache) i += is_entry_used(ce);
			return i;
		case CI_LIST:
			return (long) &cache;
		default:
			internal("cache_info: bad request");
	}
	return 0;
}

/* Return 1 and save cache entry to 'f' if there's matching one, otherwise
 * return 0. */
int
find_in_cache(unsigned char *url, struct cache_entry **f)
{
	struct cache_entry *e;

	url = extract_proxy(url);

	foreach(e, cache) {
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
	e->length = 0;
	e->incomplete = 1;
	e->data_size = 0;

	init_list(e->frag);

	e->count = cache_count++;
	e->refcount = 0;
	add_to_list(cache, e);
	*f = e;

	return 0;
}

#if 0
int get_cache_data(struct cache_entry *e, unsigned char **d, int *l)
{
	struct fragment *frag;
	*d = NULL; *l = 0;
	if ((void *)(frag = e->frag.next) == &e->frag) return -1;
	*d = frag->data;
	*l = frag->length;
	return 0;
}
#endif

#define enlarge(e, x) e->data_size += x, cache_size += x

#define CACHE_PAD(x) (((x) | 0x3fff) + 1)

/* Add fragment to cache. Returns -1 upon error, 1 if cache entry was enlarged,
 * 0 if only old data were overwritten. Maybe. And maybe not. */
/* Note that this function is maybe overcommented, but I'm certainly not
 * unhappy from that. */
int
add_fragment(struct cache_entry *e, int offset,
	     unsigned char *data, int length)
{
	int end_offset, f_end_offset;
	struct fragment *f;
	struct fragment *nf;
	int ret = 0;
	int trunc = 0;

	if (!length) return 0;

	if (e->length < offset + length)
		e->length = offset + length;

	/* XXX: This is probably some magic strange thing for HTML renderer. */
	e->count = cache_count++;

	/* Possibly insert the new data in the middle of existing fragment. */

	end_offset = offset + length;
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
				enlarge(e, end_offset - f_end_offset);
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

	nf = mem_alloc(sizeof(struct fragment) + CACHE_PAD(length));
	if (!nf) return -1;

	ret = 1;
	enlarge(e, length);
	nf->offset = offset;
	nf->length = length;
	nf->real_length = CACHE_PAD(length);
	memcpy(nf->data, data, length);
	add_at_pos(f->prev, nf);
	f = nf;

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
			nf = mem_realloc(f, sizeof(struct fragment)
					    + end_offset - f->offset);
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

			enlarge(e, end_offset - f_end_offset);
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
		del_from_list(nf);
		enlarge(e, -nf->length);
		mem_free(nf);
	}

	if (trunc) truncate_entry(e, offset + length, 0);

#if 0
	foreach(f, e->frag)
		fprintf(stderr, "%d, %d, %d\n",
			f->offset, f->length, f->real_length);
	debug("ret-");
#endif

	return ret;
}

#undef CACHE_PAD

void
defrag_entry(struct cache_entry *e)
{
	struct fragment *f, *g, *h, *n, *x;
	int l;

	if (list_empty(e->frag)) return;
	f = e->frag.next;
	if (f->offset) return;
	for (g = f->next;
	     g != (void *)&e->frag && g->offset <= g->prev->offset + g->prev->length;
	     g = g->next)
		if (g->offset < g->prev->offset + g->prev->length) {
			internal("fragments overlay");
			return;
		}

	if (g == f->next) return;

	for (l = 0, h = f; h != g; h = h->next)
		l += h->length;

	n = mem_alloc(sizeof(struct fragment) + l);
	if (!n) return;
	n->offset = 0;
	n->length = l;
	n->real_length = l;
#if 0
	{
		struct fragment *f;
		foreach(f, e->frag) fprintf(stderr, "%d, %d, %d\n", f->offset, f->length, f->real_length);
		debug("d1-");
	}
#endif
	for (l = 0, h = f; h != g; h = h->next) {
		memcpy(n->data + l, h->data, h->length);
		l += h->length;
		x = h;
		h = h->prev;
		del_from_list(x);
		mem_free(x);
	}
	add_to_list(e->frag, n);
#if 0
	{
		foreach(f, e->frag) fprintf(stderr, "%d, %d, %d\n", f->offset, f->length, f->real_length);
		debug("d-");
	}
#endif
}

void
truncate_entry(struct cache_entry *e, int off, int final)
{
	struct fragment *f, *g;

	if (e->length > off) {
		e->length = off;
		e->incomplete = 1;
	}

	foreach(f, e->frag) {
		if (f->offset >= off) {

del:
			while ((void *)f != &e->frag) {
				enlarge(e, -f->length);
				g = f->next;
				del_from_list(f);
				mem_free(f);
				f = g;
			}
			return;
		}
		if (f->offset + f->length > off) {
			f->length = off - f->offset;
			enlarge(e, -(f->offset + f->length - off));
			if (final) {
				g = mem_realloc(f, sizeof(struct fragment)
						   + f->length);
				if (g) {
					g->next->prev = g;
					g->prev->next = g;
					f = g;
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
	struct fragment *f, *g;

	foreach(f, e->frag) {
		if (f->offset + f->length <= off) {
			enlarge(e, -f->length);
			g = f;
			f = f->prev;
			del_from_list(g);
			mem_free(g);
		} else if (f->offset < off) {
			enlarge(e, f->offset - off);
			f->length -= off - f->offset;
			memmove(f->data, f->data + off - f->offset, f->length);
			f->offset = off;
		} else break;
	}
}

void
delete_entry_content(struct cache_entry *e)
{
	e->count = cache_count++;
	free_list(e->frag);
	e->length = 0;
	e->incomplete = 1;

	cache_size -= e->data_size;
	if (cache_size < 0) {
		internal("cache_size underflow: %ld", cache_size);
		cache_size = 0;
	}
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
	if (e->refcount) internal("deleting locked cache entry");
#ifdef DEBUG
	if (is_entry_used(e)) internal("deleting loading cache entry");
#endif

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
garbage_collection(int u)
{
	struct cache_entry *e, *f;
	long ncs = cache_size;
	long ccs = 0;
	int no = 0;
	long opt_cache_memory_size = get_opt_long("document.cache.memory.size");

	if (!u && cache_size <= opt_cache_memory_size) return;

	foreach(e, cache) {
		if (e->refcount || is_entry_used(e)) {
			ncs -= e->data_size;
			if (ncs < 0) {
				internal("cache_size underflow: %ld", ncs);
				ncs = 0;
			}
		}
		ccs += e->data_size;
	}

	if (ccs != cache_size) {
		internal("cache size badly computed: %ld != %ld", cache_size, ccs);
	       	cache_size = ccs;
	}

	if (!u && ncs <= opt_cache_memory_size) return;

	for (e = cache.prev; (void *)e != &cache; e = e->prev) {
		if (!u && ncs <= opt_cache_memory_size * MEMORY_CACHE_GC_PERCENT / 100)
			goto g;
		if (e->refcount || is_entry_used(e)) {
			no = 1;
			e->tgc = 0;
			continue;
		}
		e->tgc = 1;
		ncs -= e->data_size;
		if (ncs < 0) {
			internal("cache_size underflow: %ld", ncs);
			ncs = 0;
		}
	}
	if (/*!no &&*/ ncs)
		internal("cache_size(%ld) is larger than size of all objects(%ld)",
			 cache_size, cache_size - ncs);

g:
	e = e->next;
	if ((void *)e == &cache) return;

	if (!u) {
		for (f = e; (void *)f != &cache; f = f->next) {
			if (ncs + f->data_size <= opt_cache_memory_size * MEMORY_CACHE_GC_PERCENT / 100) {
				ncs += f->data_size;
				f->tgc = 0;
			}
		}
	}

	for (f = e; (void *)f != &cache;) {
		f = f->next;
		if (f->prev->tgc)
			delete_cache_entry(f->prev);
	}
#if 0
	if (!no && cache_size > get_opt_long("document.cache.memory.size") * MEMORY_CACHE_GC_PERCENT / 100) {
		internal("garbage collection doesn't work, cache size %ld", cache_size);
	}
#endif
}
