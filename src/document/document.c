/* The document base functionality */
/* $Id: document.c,v 1.62 2004/04/04 04:44:47 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "cache/cache.h"
#include "config/options.h"
#include "document/document.h"
#include "document/html/frames.h"
#include "document/html/parser.h"
#include "document/html/renderer.h"
#include "document/options.h"
#include "document/refresh.h"
#include "modules/module.h"
#include "protocol/uri.h"
#include "terminal/draw.h"
#include "util/color.h"
#include "util/lists.h"
#include "util/memory.h"
#include "util/object.h"
#include "util/string.h"
#include "viewer/text/form.h"
#include "viewer/text/link.h"


static INIT_LIST_HEAD(format_cache);
static int format_cache_entries = 0;

struct document *
init_document(struct uri *uri, struct cache_entry *cached,
	      struct document_options *options)
{
	struct document *document = mem_calloc(1, sizeof(struct document));

	if (!document) return NULL;

	document->uri = get_uri_reference(uri);

	object_lock(cached);
	document->id_tag = cached->id_tag;

	init_list(document->forms);
	init_list(document->tags);
	init_list(document->nodes);
	init_list(document->css_imports);

	object_nolock(document, "document");
	object_lock(document);

	copy_opt(&document->options, options);
	global_doc_opts = &document->options;

	add_to_list(format_cache, document);

	return document;
}

static void
free_frameset_desc(struct frameset_desc *frameset_desc)
{
	int i;

	for (i = 0; i < frameset_desc->n; i++) {
		struct frame_desc *frame_desc = &frameset_desc->frame_desc[i];

		if (frame_desc->subframe)
			free_frameset_desc(frame_desc->subframe);
		if (frame_desc->name)
			mem_free(frame_desc->name);
		if (frame_desc->uri)
			done_uri(frame_desc->uri);
	}

	mem_free(frameset_desc);
}

void
done_link_members(struct link *link)
{
	if (link->where) mem_free(link->where);
	if (link->target) mem_free(link->target);
	if (link->title) mem_free(link->title);
	if (link->where_img) mem_free(link->where_img);
	if (link->pos) mem_free(link->pos);
	if (link->name) mem_free(link->name);
}

void
done_document(struct document *document)
{
	struct cache_entry *cached;
	struct form_control *fc;
	int pos;

	assert(document);
	if_assert_failed return;

	assertm(!is_object_used(document), "Attempt to free locked formatted data.");
	if_assert_failed return;

	cached = find_in_cache(document->uri);
	if (!cached)
		INTERNAL("no cache entry for document");
	else
		object_unlock(cached);

	if (document->uri) done_uri(document->uri);
	if (document->title) mem_free(document->title);
	if (document->frame_desc) free_frameset_desc(document->frame_desc);
	if (document->refresh) done_document_refresh(document->refresh);

	for (pos = 0; pos < document->nlinks; pos++) {
		done_link_members(&document->links[pos]);
	}

	if (document->links) mem_free(document->links);

	if (document->data) {
		for (pos = 0; pos < document->height; pos++) {
			if (document->data[pos].chars)
				mem_free(document->data[pos].chars);
		}

		mem_free(document->data);
	}

	if (document->lines1) mem_free(document->lines1);
	if (document->lines2) mem_free(document->lines2);
	if (document->options.framename) mem_free(document->options.framename);

	foreach (fc, document->forms) {
		done_form_control(fc);
	}

	free_list(document->forms);
	free_list(document->tags);
	free_list(document->nodes);
	free_string_list(&document->css_imports);

	if (document->search) mem_free(document->search);
	if (document->slines1) mem_free(document->slines1);
	if (document->slines2) mem_free(document->slines2);

	/* Blast off global document option pointer if we are the `owner'
	 * so we don't have a dangling pointer. */
	if (global_doc_opts == &document->options)
		global_doc_opts = NULL;

	del_from_list(document);
	mem_free(document);
}

void
release_document(struct document *document)
{
	assert(document);
	if_assert_failed return;

	if (document->refresh) kill_document_refresh(document->refresh);
	object_unlock(document);
	if (!is_object_used(document)) format_cache_entries++;
	del_from_list(document);
	add_to_list(format_cache, document);
}

/* Formatted document cache management */

struct document *
get_cached_document(struct uri *uri, struct document_options *options,
		    unsigned int id)
{
	struct document *document;

	foreach (document, format_cache) {
		if (document->uri != uri
		    || compare_opt(&document->options, options))
			continue;

		if (id != document->id_tag) {
			if (!is_object_used(document)) {
				document = document->prev;
				done_document(document->next);
				format_cache_entries--;
			}
			continue;
		}

		/* Reactivate */
		del_from_list(document);
		add_to_list(format_cache, document);

		if (!is_object_used(document))
			format_cache_entries--;

		object_lock(document);

		return document;
	}

	return NULL;
}

void
shrink_format_cache(int whole)
{
	struct document *document;
	int format_cache_size = get_opt_int("document.cache.format.size");

#ifdef CONFIG_DEBUG
	{
		int entries = 0;

		foreach (document, format_cache)
			if (!is_object_used(document)) entries++;

		assertm(entries == format_cache_entries,
			"format_cache_entries out of sync (%d != %d)",
			entries, format_cache_entries);
	}
#endif

	foreach (document, format_cache) {
		struct cache_entry *cached;

		if (is_object_used(document)) continue;

		/* Destroy obsolete renderer documents which are already
		 * out-of-sync. */
		cached = find_in_cache(document->uri);
		assertm(cached, "cached formatted document has no cache entry");
		if (cached->id_tag == document->id_tag) continue;

		document = document->prev;
		done_document(document->next);
		format_cache_entries--;
	}

	assertm(format_cache_entries >= 0, "format_cache_entries underflow on entry");
	if_assert_failed format_cache_entries = 0;

	foreachback (document, format_cache) {
		if (is_object_used(document)) continue;

		/* If we are not purging the whole format cache, stop
		 * once we are below the maximum number of entries. */
		if (!whole && format_cache_entries <= format_cache_size)
			break;

		/* Jump back to already processed entry (or list head), and let
		 * the foreachback move it to the next entry to go. */
		document = document->next;
		done_document(document->prev);
		format_cache_entries--;
	}

	assertm(format_cache_entries >= 0, "format_cache_entries underflow");
	if_assert_failed format_cache_entries = 0;
}

long
formatted_info(int type)
{
	int i = 0;
	struct document *document;

	switch (type) {
		case INFO_FILES:
			foreach (document, format_cache) i++;
			return i;
		case INFO_LOCKED:
			foreach (document, format_cache)
				i += is_object_used(document);
			return i;
	}

	return 0;
}

static void
init_documents(struct module *module)
{
	init_tags_lookup();
}

static void
done_documents(struct module *module)
{
	free_tags_lookup();
	free_table_cache();
}

struct module document_module = struct_module(
	/* name: */		"Document",
	/* options: */		NULL,
	/* hooks: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		init_documents,
	/* done: */		done_documents
);
