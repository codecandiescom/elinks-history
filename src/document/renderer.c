/* HTML renderer */
/* $Id: renderer.c,v 1.69 2004/08/13 20:54:43 jonas Exp $ */

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
#include "document/renderer.h"
#include "document/view.h"
#include "intl/charsets.h"
#include "protocol/header.h"
#include "protocol/uri.h"
#include "protocol/protocol.h"
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
	struct cache_entry *cached;

	assert(vs && doc_view && options);
	if_assert_failed return;

	name = doc_view->name;
	doc_view->name = NULL;

	detach_formatted(doc_view);

	doc_view->name = name;
	doc_view->vs = vs;
	doc_view->last_x = doc_view->last_y = -1;

	cached = find_in_cache(vs->uri);
	if (!cached) {
		INTERNAL("document %s to format not found", struri(vs->uri));
		return;
	}

	document = get_cached_document(cached, options);
	if (!document) {
		struct fragment *fr;

		document = init_document(cached, options);
		if (!document) return;

		shrink_memory(0);

		defrag_entry(cached);
		fr = cached->frag.next;

		if (list_empty(cached->frag) || fr->offset || !fr->length) {

		} else if (document->options.plain) {
			render_plain_document(cached, document);

		} else {
			render_html_document(cached, document);
		}

		sort_links(document);
		if (!document->title) {
			if (document->uri->protocol == PROTOCOL_FILE) {
				document->title = get_uri_string(document->uri,
								 URI_PATH);
				decode_uri_string(document->title);
			} else {
				document->title = get_uri_string(document->uri,
								 URI_PUBLIC);
			}
		}

		document->css_magic = get_document_css_magic(document);
	}

	doc_view->document = document;

	/* If we do not care about the height and width of the document
	 * just use the setup values. */

	copy_box(&doc_view->box, &document->options.box);

	if (!document->options.needs_width)
		doc_view->box.width = options->box.width;

	if (!document->options.needs_height)
		doc_view->box.height = options->box.height;
}


void
render_document_frames(struct session *ses, int no_cache)
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

	set_box(&doc_opts.box, 0, 0,
		ses->tab->term->width, ses->tab->term->height);

	if (ses->status.show_title_bar) {
		doc_opts.box.y++;
		doc_opts.box.height--;
	}
	if (ses->status.show_status_bar) doc_opts.box.height--;
	if (ses->status.show_tabs_bar) doc_opts.box.height--;

	doc_opts.color_mode = get_opt_int_tree(ses->tab->term->spec, "colors");
	if (!get_opt_int_tree(ses->tab->term->spec, "underline"))
		doc_opts.color_flags |= COLOR_ENHANCE_UNDERLINE;

	doc_opts.cp = get_opt_int_tree(ses->tab->term->spec, "charset");
	doc_opts.no_cache = no_cache;

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
	return (l1->number - l2->number);
}

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
		int p, q, j;

		if (!link->npoints) {
			done_link_members(link);
			memmove(link, link + 1,
				(document->nlinks - i - 1) * sizeof(struct link));
			document->nlinks--;
			i--;
			continue;
		}
		p = link->points[0].y;
		q = link->points[link->npoints - 1].y;
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

struct conv_table *
get_convert_table(unsigned char *head, int to_cp,
		  int default_cp, int *from_cp,
		  enum cp_status *cp_status, int ignore_server_cp)
{
	unsigned char *part = head;
	int cp_index = -1;

	assert(head);
	if_assert_failed return NULL;

	if (ignore_server_cp) {
		if (cp_status) *cp_status = CP_STATUS_IGNORED;
		if (from_cp) *from_cp = default_cp;
		return get_translation_table(default_cp, to_cp);
	}

	while (cp_index == -1) {
		unsigned char *ct_charset;
		unsigned char *a = parse_header(part, "Content-Type", &part);

		if (!a) break;

		ct_charset = parse_header_param(a, "charset");
		if (ct_charset) {
			cp_index = get_cp_index(ct_charset);
			mem_free(ct_charset);
		}
		mem_free(a);
	}

	if (cp_index == -1) {
		unsigned char *a = parse_header(head, "Content-Charset", NULL);

		if (a) {
			cp_index = get_cp_index(a);
			mem_free(a);
		}
	}

	if (cp_index == -1) {
		unsigned char *a = parse_header(head, "Charset", NULL);

		if (a) {
			cp_index = get_cp_index(a);
			mem_free(a);
		}
	}

	if (cp_index == -1) {
		cp_index = default_cp;
		if (cp_status) *cp_status = CP_STATUS_ASSUMED;
	} else {
		if (cp_status) *cp_status = CP_STATUS_SERVER;
	}

	if (from_cp) *from_cp = cp_index;

	return get_translation_table(cp_index, to_cp);
}

