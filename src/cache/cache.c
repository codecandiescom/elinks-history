/* Cache subsystem */
/* $Id: cache.c,v 1.123 2004/04/03 01:35:51 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "bfu/listbox.h"
#include "cache/cache.h"
#include "cache/dialogs.h"
#include "config/options.h"
#include "main.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "sched/connection.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/object.h"
#include "util/string.h"
#include "util/types.h"

/* The list of cache entries */
static INIT_LIST_HEAD(cache);

static long cache_size;
static int id_tag_counter = 0;


/* Change 0 to 1 to enable cache debugging features (redirect stderr to a file). */
#if 0
#define DEBUG_CACHE
#endif

#ifdef DEBUG_CACHE

#define dump_frag(frag, count) \
do { \
	DBG(" [%d] f=%p offset=%li length=%li real_length=%li", \
	      count, frag, frag->offset, frag->length, frag->real_length); \
} while (0)

#define dump_frags(entry, comment) \
do { \
	struct fragment *frag; \
        int count = 0;	\
 \
	DBG("%s: url=%s, cache_size=%li", comment, entry->url, cache_size); \
	foreach (frag, entry->frag) \
		dump_frag(frag, ++count); \
} while (0)

#else
#define dump_frags(entry, comment)
#endif /* DEBUG_CACHE */


static int
is_entry_used(struct cache_entry *ce)
{
	struct connection *conn;

	foreach (conn, queue)
		if (conn->cache == ce)
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
			foreach (ce, cache) i += is_object_used(ce);
			return i;
		case INFO_LOADING:
			foreach (ce, cache) i += is_entry_used(ce);
			return i;
		case INFO_LIST:
			return (long) &cache;
	}
	return 0;
}

struct cache_entry *
find_in_cache(struct uri *uri)
{
	struct cache_entry *ce;
	struct cache_entry *found = NULL;
	struct uri *proxy_uri = NULL;

	/* If only the caller has a reference it will most definitely not be in
	 * the cache unless the caller uses an URI from a cache_entry in which
	 * case the coder is one twisted fsck. */
	if (get_object_refcount(uri) == 1)
		return NULL;

	if (uri->protocol == PROTOCOL_PROXY) {
		uri = proxy_uri = get_proxied_uri(uri);
		if (!proxy_uri) return NULL;
	}

	foreach (ce, cache) {
		assert(get_cache_uri_struct(ce) && uri);
		if (!ce->valid || ce->uri != uri) continue;

		/* Move it on the top of the list. */
		del_from_list(ce);
		add_to_list(cache, ce);

		found = ce;
		break;
	}

	if (proxy_uri) done_uri(proxy_uri);

	return found;
}

struct cache_entry *
get_cache_entry(struct uri *uri)
{
	struct cache_entry *ce = find_in_cache(uri);

	if (ce) return ce;

	shrink_memory(0);

	ce = mem_calloc(1, sizeof(struct cache_entry));
	if (!ce) return NULL;

	ce->uri = get_proxied_uri(uri);
	if (!ce->uri) {
		mem_free(ce);
		return NULL;
	}

	ce->incomplete = 1;
	ce->valid = 1;
	init_list(ce->frag);
	ce->id_tag = id_tag_counter++;
	object_nolock(ce); /* Debugging purpose. */

	add_to_list(cache, ce);

	ce->box_item = add_listbox_item(&cache_browser, struri(ce->uri), ce);

	return ce;
}

static inline void
enlarge_entry(struct cache_entry *ce, int size)
{
	ce->data_size += size;
	assertm(ce->data_size >= 0, "cache entry data_size underflow: %ld", ce->data_size);
	if_assert_failed { ce->data_size = 0; }

	cache_size += size;
	assertm(cache_size >= 0, "cache_size underflow: %ld", cache_size);
	if_assert_failed { cache_size = 0; }
}

#define CACHE_PAD(x) (((x) | 0x3fff) + 1)

/* One byte is reserved for data in struct fragment. */
#define FRAGSIZE(x) (sizeof(struct fragment) + (x) - 1)

/* Add fragment to cache. Returns -1 upon error, 1 if cache entry was enlarged,
 * 0 if only old data were overwritten. Maybe. And maybe not. */
/* Note that this function is maybe overcommented, but I'm certainly not
 * unhappy from that. */
int
add_fragment(struct cache_entry *ce, int offset,
	     unsigned char *data, int length)
{
	struct fragment *f, *nf;
	int ret = 0;
	int trunc = 0;
	int end_offset, f_end_offset;

	if (!length) return 0;

	end_offset = offset + length;
	if (ce->length < end_offset)
		ce->length = end_offset;

	/* id_tag marks each entry, and change each time it's modified,
	 * used in HTML renderer. */
	ce->id_tag = id_tag_counter++;

	/* Possibly insert the new data in the middle of existing fragment. */
	foreach (f, ce->frag) {
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
				enlarge_entry(ce, end_offset - f_end_offset);
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
	nf = mem_calloc(1, FRAGSIZE(CACHE_PAD(length)));
	if (!nf) return -1;

	nf->offset = offset;
	nf->length = length;
	nf->real_length = CACHE_PAD(length);
	memcpy(nf->data, data, length);
	add_at_pos(f->prev, nf);
	f = nf;

	ret = 1;
	enlarge_entry(ce, length);

remove_overlaps:
	/* Contatenate overlapping fragments. */

	f_end_offset = f->offset + f->length;
	/* Iterate thru all fragments we still overlap to. */
	while ((void *) f->next != &ce->frag
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

			enlarge_entry(ce, end_offset - f_end_offset);
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
		enlarge_entry(ce, -nf->length);
		del_from_list(nf);
		mem_free(nf);
	}

	if (trunc) truncate_entry(ce, offset + length, 0);

	dump_frags(ce, "add_fragment");

	return ret;
}

void
defrag_entry(struct cache_entry *ce)
{
	struct fragment *first_frag, *adj_frag, *frag, *new_frag;
	int new_frag_len;

	if (list_empty(ce->frag)) return;
	first_frag = ce->frag.next;
	if (first_frag->offset) return;

	for (adj_frag = first_frag->next; adj_frag != (void *) &ce->frag;
	     adj_frag = adj_frag->next) {
		long overlay = adj_frag->offset
				- (adj_frag->prev->offset
				   + adj_frag->prev->length);

		if (overlay > 0) break;
		if (overlay == 0) continue;

		INTERNAL("fragments overlay");
		return;
	}

	if (adj_frag == first_frag->next) return;

	for (new_frag_len = 0, frag = first_frag;
	     frag != adj_frag;
	     frag = frag->next)
		new_frag_len += frag->length;

	/* One byte is reserved for data in struct fragment. */
	new_frag = mem_calloc(1, FRAGSIZE(new_frag_len));
	if (!new_frag) return;
	new_frag->length = new_frag_len;
	new_frag->real_length = new_frag_len;

	for (new_frag_len = 0, frag = first_frag;
	     frag != adj_frag;
	     frag = frag->next) {
		struct fragment *tmp = frag;

		memcpy(new_frag->data + new_frag_len, frag->data, frag->length);
		new_frag_len += frag->length;

		frag = frag->prev;
		del_from_list(tmp);
		mem_free(tmp);
	}

	add_to_list(ce->frag, new_frag);

	dump_frags(ce, "defrag_entry");
}

void
truncate_entry(struct cache_entry *ce, int off, int final)
{
	struct fragment *f;

	if (ce->length > off) {
		ce->length = off;
		ce->incomplete = 1;
	}

	foreach (f, ce->frag) {
		long size = off - f->offset;

		if (size <= 0) {

del:
			while ((void *)f != &ce->frag) {
				struct fragment *tmp = f->next;

				enlarge_entry(ce, -f->length);
				del_from_list(f);
				mem_free(f);
				f = tmp;
			}
			dump_frags(ce, "truncate_entry");
			return;
		}

		if (f->length > size) {
			enlarge_entry(ce, -(f->length - size));
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
free_entry_to(struct cache_entry *ce, int off)
{
	struct fragment *f;

	foreach (f, ce->frag) {
		if (f->offset + f->length <= off) {
			struct fragment *tmp = f;

			enlarge_entry(ce, -f->length);
			f = f->prev;
			del_from_list(tmp);
			mem_free(tmp);
		} else if (f->offset < off) {
			long size = off - f->offset;

			enlarge_entry(ce, -size);
			f->length -= size;
			memmove(f->data, f->data + size, f->length);
			f->offset = off;
		} else break;
	}
}

void
delete_entry_content(struct cache_entry *ce)
{
	enlarge_entry(ce, -ce->data_size);

	free_list(ce->frag);
	ce->id_tag = id_tag_counter++;
	ce->length = 0;
	ce->incomplete = 1;

	if (ce->last_modified) {
		mem_free(ce->last_modified);
		ce->last_modified = NULL;
	}

	if (ce->etag) {
		mem_free(ce->etag);
		ce->etag = NULL;
	}
}

void
delete_cache_entry(struct cache_entry *ce)
{
	assertm(!is_object_used(ce), "deleting locked cache entry");
	assertm(!is_entry_used(ce), "deleting loading cache entry");

	delete_entry_content(ce);
	del_from_list(ce);

	if (ce->box_item) done_listbox_item(&cache_browser, ce->box_item);
	if (ce->uri) done_uri(ce->uri);
	if (ce->head) mem_free(ce->head);
	if (ce->last_modified) mem_free(ce->last_modified);
	if (ce->redirect) mem_free(ce->redirect);
	if (ce->ssl_info) mem_free(ce->ssl_info);
	if (ce->encoding_info) mem_free(ce->encoding_info);
	if (ce->etag) mem_free(ce->etag);

	mem_free(ce);
}


struct uri *
get_cache_redirect_uri(struct cache_entry *entry)
{
	/* XXX: I am a little puzzled whether we should only use the cache
	 * entry's URI if it is valid. Hopefully always using it won't hurt
	 * --jonas */
	struct uri *base = entry->uri;
	unsigned char *basestring = base ? struri(base) : NULL;
	unsigned char *uristring = empty_string_or_(basestring);
	struct uri *uri;

	uristring = join_urls(uristring, entry->redirect);
	if (!uristring) return NULL;

	/* According to RFC2068 POST must not be redirected to GET,
	 * but some BUGGY message boards rely on it :-( */
	if (base
	    && base->post
	    && !entry->redirect_get
	    && !get_opt_int("protocol.http.bugs.broken_302_redirect")) {
		/* XXX: Add POST_CHAR and post data assuming URI components
		 * belong to one string. */
		add_to_strn(&uristring, base->post - 1);
	}

	uri = get_uri(uristring, -1);
	mem_free(uristring);

	return uri;
}

int
redirect_cache(struct cache_entry *cache, unsigned char *location,
	       int get, int incomplete)
{
	if (cache->redirect) mem_free(cache->redirect);

	cache->redirect = straconcat(struri(cache->uri), location, NULL);
	cache->redirect_get = get;
	if (incomplete >= 0) cache->incomplete = incomplete;

	return !!cache->redirect;
}


void
garbage_collection(int whole)
{
	struct cache_entry *ce;
	/* We recompute cache_size when scanning cache entries, to ensure
	 * consistency. */
	long old_cache_size = 0;
	/* The maximal cache size tolerated by user. Note that this is only
	 * size of the "just stored" unused cache entries, used cache entries
	 * are not counted to that. */
	long opt_cache_size = get_opt_long("document.cache.memory.size");
	/* The low-treshold cache size. Basically, when the cache size is
	 * higher than opt_cache_size, we free the cache so that there is no
	 * more than this value in the cache anymore. This is to make sure we
	 * aren't cleaning cache too frequently when working with a lot of
	 * small cache entries but rather free more and then let it grow a
	 * little more as well. */
	long gc_cache_size = opt_cache_size * MEMORY_CACHE_GC_PERCENT / 100;
	/* The cache size we aim to reach. */
	long new_cache_size = cache_size;
#ifdef DEBUG_CACHE
	/* Whether we've hit an used (unfreeable) entry when collecting
	 * garbage. */
	int obstacle_entry = 0;
#endif

#ifdef DEBUG_CACHE
	DBG("gc whole=%d opt_cache_size=%ld gc_cache_size=%ld",
	      whole, opt_cache_size,gc_cache_size);
#endif

	if (!whole && cache_size <= opt_cache_size) return;


	/* Scanning cache, pass #1:
	 * Weed out the used cache entries from @new_cache_size, so that we
	 * will work only with the unused entries from then on. Also ensure
	 * that @cache_size is in sync. */

	foreach (ce, cache) {
		old_cache_size += ce->data_size;

		if (!is_object_used(ce) && !is_entry_used(ce))
			continue;

		new_cache_size -= ce->data_size;

		assertm(new_cache_size >= 0,
				"cache_size (%ld) underflow: %ld",
				cache_size, new_cache_size);
		if_assert_failed { new_cache_size = 0; }
	}

	assertm(old_cache_size == cache_size,
		"cache_size out of sync: %ld != (actual) %ld",
		cache_size, old_cache_size);
	if_assert_failed { cache_size = old_cache_size; }

	if (!whole && new_cache_size <= opt_cache_size) return;


	/* Scanning cache, pass #2:
	 * Mark potential targets for destruction, from the oldest to the
	 * newest. */

	foreachback (ce, cache) {
		/* We would have shrinked enough already? */
		if (!whole && new_cache_size <= gc_cache_size)
			goto shrinked_enough;

		/* Skip used cache entries. */
		if (is_object_used(ce) || is_entry_used(ce)) {
#ifdef DEBUG_CACHE
			obstacle_entry = 1;
#endif
			ce->gc_target = 0;
			continue;
		}

		/* Mark me for destruction, sir. */
		ce->gc_target = 1;
		new_cache_size -= ce->data_size;

		assertm(new_cache_size >= 0,
			"cache_size (%ld) underflow: %ld",
			cache_size, new_cache_size);
		if_assert_failed { new_cache_size = 0; }
	}

	/* If we'd free the whole cache... */
	assertm(new_cache_size == 0,
		"cache_size (%ld) overflow: %ld",
		cache_size, new_cache_size);
	if_assert_failed { new_cache_size = 0; }

shrinked_enough:


	/* Now turn around and start walking in the opposite direction. */
	ce = ce->next;

	/* Something is strange when we decided all is ok before dropping any
	 * cache entry. */
	if ((void *) ce == &cache) return;


	if (!whole) {
		struct cache_entry *entry;

		/* Scanning cache, pass #3:
		 * Walk back in the cache and unmark the cache entries which
		 * could still fit into the cache. */

		/* This makes sense when the newest entry is HUGE and after it,
		 * there's just plenty of tiny entries. By this point, all the
		 * tiny entries would be marked for deletion even though it'd
		 * be enough to free the huge entry. This actually fixes that
		 * situation. */

		for (entry = ce; (void *) entry != &cache; entry = entry->next) {
			long newer_cache_size = new_cache_size + entry->data_size;

			if (newer_cache_size > gc_cache_size)
				continue;

			new_cache_size = newer_cache_size;
			entry->gc_target = 0;
		}
	}


	/* Scanning cache, pass #4:
	 * Destroy the marked entries. So sad, but that's life, bro'. */

	for (; (void *) ce != &cache; ) {
		ce = ce->next;
		if (ce->prev->gc_target)
			delete_cache_entry(ce->prev);
	}


#ifdef DEBUG_CACHE
	if ((whole || !obstacle_entry) && cache_size > gc_cache_size) {
		DBG("garbage collection doesn't work, cache size %ld > %ld, "
		      "document.cache.memory.size set to: %ld bytes",
		      cache_size, gc_cache_size,
		      get_opt_long("document.cache.memory.size"));
	}
#endif
}
