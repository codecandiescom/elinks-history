/* HTML renderer */
/* $Id: renderer.c,v 1.33 2004/03/22 04:51:00 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "main.h"
#include "cache/cache.h"
#include "config/options.h"
#include "document/document.h"
#include "document/html/frames.h"
#include "document/html/renderer.h"
#include "document/plain/renderer.h"
#include "document/view.h"
#include "sched/session.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/object.h"
#include "util/string.h"
#include "viewer/text/view.h"
#include "viewer/text/vs.h"


static void sort_links(struct document *document);

void
render_document(struct view_state *vs, struct document_view *doc_view,
		struct document_options *options)
{
	unsigned char *name;
	struct document *document;
	struct cache_entry *cache_entry;

	assert(vs && doc_view && options);
	if_assert_failed return;

	name = doc_view->name;
	doc_view->name = NULL;

	detach_formatted(doc_view);

	doc_view->name = name;
	doc_view->vs = vs;
	doc_view->last_x = doc_view->last_y = -1;

	cache_entry = get_vs_cache_entry(vs);
	if (!cache_entry) {
		INTERNAL("document %s to format not found", struri(vs->uri));
		return;
	}

	document = get_cached_document(struri(vs->uri), options, cache_entry->id_tag);
	if (!document) {
		struct fragment *fr;

		document = init_document(vs->uri, cache_entry, options);
		if (!document) return;

		shrink_memory(0);

		defrag_entry(cache_entry);
		fr = cache_entry->frag.next;

		if (list_empty(cache_entry->frag) || fr->offset || !fr->length) {
			/* m33p */

		} else if (document->options.plain) {
			render_plain_document(cache_entry, document);

		} else {
			render_html_document(cache_entry, document);
		}

		if (!document->title) {
			/* FIXME: Remove user and password too? --jonas */
			document->title = get_uri_string(document->uri, ~URI_POST);
		}

		sort_links(document);
	}

	doc_view->document = document;
	doc_view->x = document->options.x;
	doc_view->y = document->options.y;

	/* If we do not care about the height and width of the document
	 * just use the setup values. */
	doc_view->width = document->options.needs_width
			? document->options.width : options->width;
	doc_view->height = document->options.needs_height
			 ? document->options.height : options->height;
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

	doc_opts.x = 0;
	doc_opts.y = 0;
	doc_opts.width = ses->tab->term->width;
	doc_opts.height = ses->tab->term->height;

	if (ses->status.show_title_bar) {
		doc_opts.y++;
		doc_opts.height--;
	}
	if (ses->status.show_status_bar) doc_opts.height--;
	if (ses->status.show_tabs_bar) doc_opts.height--;

	doc_opts.color_mode = get_opt_int_tree(ses->tab->term->spec, "colors");
	if (!get_opt_int_tree(ses->tab->term->spec, "underline"))
		doc_opts.color_flags |= COLOR_ENHANCE_UNDERLINE;

	doc_opts.cp = get_opt_int_tree(ses->tab->term->spec, "charset");

	if (vs) {
		if (vs->plain < 0) vs->plain = 0;
		doc_opts.plain = vs->plain;
		doc_opts.wrap = vs->wrap;
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


static int
comp_links(struct link *l1, struct link *l2)
{
	assert(l1 && l2);
	if_assert_failed return 0;
	return (l1->num - l2->num);
}

#if 0
static int
comp_links(struct link *l1, struct link *l2)
{
	int res;

	assert(l1 && l2 && l1->pos && l2->pos);
	if_assert_failed return 0;
	res = l1->pos->y - l2->pos->y;
	if (res) return res;
	return l1->pos->x - l2->pos->x;
}
#endif

static void
sort_links(struct document *document)
{
	int i;

	assert(document);
	if_assert_failed return;
	if (!document->nlinks) return;

	assert(document->links);
	if_assert_failed return;

	qsort(document->links, document->nlinks, sizeof(struct link),
	      (void *) comp_links);

	if (!document->height) return;

	document->lines1 = mem_calloc(document->height, sizeof(struct link *));
	if (!document->lines1) return;
	document->lines2 = mem_calloc(document->height, sizeof(struct link *));
	if (!document->lines2) {
		mem_free(document->lines1);
		return;
	}

	for (i = 0; i < document->nlinks; i++) {
		struct link *link = &document->links[i];
		register int p, q, j;

		if (!link->n) {
			done_link_members(link);
			memmove(link, link + 1,
				(document->nlinks - i - 1) * sizeof(struct link));
			document->nlinks--;
			i--;
			continue;
		}
		p = link->pos[0].y;
		q = link->pos[link->n - 1].y;
		if (p > q) j = p, p = q, q = j;
		for (j = p; j <= q; j++) {
			assertm(j < document->height, "link out of screen");
			if_assert_failed continue;
			document->lines2[j] = &document->links[i];
			if (!document->lines1[j])
				document->lines1[j] = &document->links[i];
		}
	}
}
