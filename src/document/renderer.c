/* HTML renderer */
/* $Id: renderer.c,v 1.103 2004/09/28 13:55:13 pasky Exp $ */

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
#include "document/dom/renderer.h"
#include "document/html/frames.h"
#include "document/html/renderer.h"
#include "document/plain/renderer.h"
#include "document/renderer.h"
#include "document/view.h"
#include "ecmascript/ecmascript.h"
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

#ifdef CONFIG_ECMASCRIPT
/* XXX: This function is de facto obsolete, since we do not need to copy
 * snippets around anymore (we process them in one go after the document is
 * loaded; gradual processing was practically impossible because the snippets
 * could reorder randomly during the loading - consider i.e.
 * <body onLoad><script></body>: first just <body> is loaded, but then the
 * rest of the document is loaded and <script> gets before <body>; do not even
 * imagine the trouble with rewritten (through scripting hooks) documents;
 * besides, implementing document.write() will be much simpler).
 * But I want to take no risk by reworking that now. --pasky */
static void
add_snippets(struct ecmascript_interpreter *interpreter,
             struct list_head *doc_snippets, struct list_head *queued_snippets)
{
	struct string_list_item *doc_current = NULL;

#ifdef CONFIG_LEDS
	if (list_empty(*queued_snippets) && interpreter->vs->doc_view->session)
		interpreter->vs->doc_view->session->status.ecmascript_led->value = '-';
#endif

	if (list_empty(*doc_snippets) || !get_opt_bool("ecmascript.enable"))
		return;

#if 0
	/* Position @doc_current in @doc_snippet to match the end of
	 * @queued_snippets. */
	if (list_empty(*queued_snippets)) {
		doc_current = doc_snippets->next;
	} else {
		struct string_list_item *iterator = queued_snippets->next;

		doc_current = doc_snippets->next;
		assert(!list_empty(*queued_snippets));
		while (iterator != (struct string_list_item *) queued_snippets) {
			if (doc_current == (struct string_list_item *) doc_snippets) {
				INTERNAL("add_snippets(): doc_snippets shorter than queued_snippets!");
				return;
			}
			assert(!strlcmp(iterator->string.source,
			                iterator->string.length,
			                doc_current->string.source,
			                doc_current->string.length));

			doc_current = doc_current->next;
			iterator = iterator->next;
		}
	}
#else
	/* We do this only once per document now. */
	assert(list_empty(*queued_snippets));
#endif

	assert(doc_current);
	for (; doc_current != (struct string_list_item *) doc_snippets;
	     doc_current = doc_current->next) {
		add_to_string_list(queued_snippets, doc_current->string.source,
		                   doc_current->string.length);
	}
}

static void
process_snippets(struct ecmascript_interpreter *interpreter,
                 struct list_head *snippets, struct string_list_item **current)
{
	if (!*current)
		*current = snippets->next;
	for (; *current != (struct string_list_item *) snippets;
	     (*current) = (*current)->next) {
		/* TODO: Support for external references. --pasky */
		ecmascript_eval(interpreter, &(*current)->string);
	}
}
#endif

static void
render_encoded_document(struct cache_entry *cached, struct document *document)
{
	struct fragment *fr = cached->frag.next;
	struct uri *uri = cached->uri;
	enum stream_encoding encoding = ENCODING_NONE;
	struct string buffer = INIT_STRING(fr->data, fr->length);
	unsigned char *extension;

	if (list_empty(cached->frag) || fr->offset || !fr->length)
		return;

	extension = get_extension_from_uri(uri);
	if (extension) {
		encoding = guess_encoding(extension);
		mem_free(extension);
	}

	if (encoding != ENCODING_NONE) {
		int length = 0;
		unsigned char *source;

		source = decode_encoded_buffer(encoding, buffer.source,
					       buffer.length, &length);
		if (source) {
			buffer.source = source;
			buffer.length = length;
		} else {
			encoding = ENCODING_NONE;
		}
	}

	if (document->options.plain) {
#ifdef CONFIG_DOM
		if (cached->content_type
		    && !strlcasecmp("text/html", 9, cached->content_type, -1))
			render_dom_document(cached, document, &buffer);
		else
#endif
			render_plain_document(cached, document, &buffer);

	} else {
		render_html_document(cached, document, &buffer);
	}

	if (encoding != ENCODING_NONE) {
		done_string(&buffer);
	}
}

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
	assert(!vs->doc_view);
	vs->doc_view = doc_view;

#ifdef CONFIG_ECMASCRIPT
	if (vs->ecmascript_fragile)
		ecmascript_reset_state(vs);
	assert(vs->ecmascript);
#endif

	cached = find_in_cache(vs->uri);
	if (!cached) {
		INTERNAL("document %s to format not found", struri(vs->uri));
		return;
	}

	document = get_cached_document(cached, options);
	if (document) {
		doc_view->document = document;
	} else {
		document = init_document(cached, options);
		if (!document) return;
		doc_view->document = document;

		shrink_memory(0);

		defrag_entry(cached);

		render_encoded_document(cached, document);
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

#ifdef CONFIG_CSS
		document->css_magic = get_document_css_magic(document);
#endif
	}
#ifdef CONFIG_ECMASCRIPT
	assert(vs->ecmascript);
	if (!document->options.gradual_rerendering) {
		/* Passing of the onload_snippets pointers gives *_snippets()
		 * some feeling of universality, shall we ever get any other
		 * snippets (?). */
		add_snippets(vs->ecmascript,
		             &document->onload_snippets,
		             &vs->ecmascript->onload_snippets);
		process_snippets(vs->ecmascript, &vs->ecmascript->onload_snippets,
		                 &vs->ecmascript->current_onload_snippet);
	}
#endif

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
		ses->doc_view->session = ses;
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
	doc_opts.no_cache = no_cache == 1;
	doc_opts.gradual_rerendering = no_cache == 2;

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

