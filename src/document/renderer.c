/* HTML renderer */
/* $Id: renderer.c,v 1.4 2003/11/13 23:40:55 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <stdarg.h>
#include <string.h>

#include "elinks.h"

#include "main.h"
#include "config/options.h"
#include "cache/cache.h"
#include "document/document.h"
#include "document/options.h"
#include "document/html/frames.h"
#include "document/html/renderer.h"
#include "document/plain/renderer.h"
#include "sched/session.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/link.h"
#include "viewer/text/view.h"
#include "viewer/text/vs.h"


void
render_document(struct view_state *vs, struct document_view *document_view,
		struct document_options *options)
{
	unsigned char *name;
	struct document *document;
	struct cache_entry *cache_entry = NULL;

	assert(vs && document_view && options);
	if_assert_failed return;

	name = document_view->name;
	document_view->name = NULL;
	detach_formatted(document_view);

	document_view->name = name;
	document_view->link_bg = NULL;
	document_view->link_bg_n = 0;

	document_view->vs = vs;
	document_view->last_x = document_view->last_y = -1;
	document_view->document = NULL;

	if (!find_in_cache(vs->url, &cache_entry) || !cache_entry) {
		internal("document %s to format not found", vs->url);
		return;
	}

	document = get_cached_document(vs->url, options, cache_entry->id_tag);
	if (!document) {
		cache_entry_lock(cache_entry);
		shrink_memory(0);

		document = init_document(vs->url, options);
		if (!document) {
			cache_entry_unlock(cache_entry);
			return;
		}

		global_doc_opts = &document->options;
		document->id_tag = cache_entry->id_tag;
		defrag_entry(cache_entry);

		if (document->options.plain) {
			render_plain_document(cache_entry, document);
		} else {
			render_html_document(cache_entry, document);
		}

		sort_links(document);
	}

	document_view->document = document;
	document_view->width = document->options.width;
	document_view->height = document->options.height;
	document_view->x = document->options.x;
	document_view->y = document->options.y;
}


void
render_document_frames(struct session *ses)
{
	struct document_options doc_opts;
	struct document_view *doc_view;
	struct document_view *current_doc_view = NULL;
	struct view_state *vs = NULL;

	if (!ses->doc_view) {
		ses->doc_view = mem_calloc(1, sizeof(struct document_view));
		if (!ses->doc_view) return;
		ses->doc_view->search_word = &ses->search_word;
	}

	if (have_location(ses)) vs = &cur_loc(ses)->vs;

	init_document_options(&doc_opts);

	/* XXX: Sets 0.height and 0.width so keep after init_document_options(). */
	init_bars_status(ses, NULL, &doc_opts);

	doc_opts.color_mode = get_opt_int_tree(ses->tab->term->spec, "colors");
	if (!get_opt_int_tree(ses->tab->term->spec, "underline"))
		doc_opts.color_flags |= COLOR_ENHANCE_UNDERLINE;

	doc_opts.cp = get_opt_int_tree(ses->tab->term->spec, "charset");

	if (vs) {
		if (vs->plain < 0) vs->plain = 0;
		doc_opts.plain = vs->plain;
	} else {
		doc_opts.plain = 1;
	}

	foreach (doc_view, ses->scrn_frames) doc_view->used = 0;

	if (vs) render_document(vs, ses->doc_view, &doc_opts);

	if (document_has_frames(ses->doc_view->document)) {
		current_doc_view = current_frame(ses);
		format_frames(ses, ses->doc_view->document->frame_desc, &doc_opts, 0);
	}

	foreach (doc_view, ses->scrn_frames) {
		struct document_view *prev_doc_view = doc_view->prev;

		if (doc_view->used) continue;

		detach_formatted(doc_view);
		del_from_list(doc_view);
		mem_free(doc_view);
		doc_view = prev_doc_view;
	}

	if (current_doc_view) {
		int n = 0;

		foreach (doc_view, ses->scrn_frames) {
			if (document_has_frames(doc_view->document)) continue;
			if (doc_view == current_doc_view) {
				cur_loc(ses)->vs.current_link = n;
				break;
			}
			n++;
		}
	}
}
