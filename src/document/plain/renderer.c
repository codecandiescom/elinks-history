/* Plain text document renderer */
/* $Id: renderer.c,v 1.47 2003/12/22 02:10:11 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <string.h>

#include "elinks.h"
#include "main.h"

#include "cache/cache.h"
#include "document/docdata.h"
#include "document/document.h"
#include "document/html/renderer.h" /* TODO: Move get_convert_table() */
#include "document/plain/renderer.h"
#include "intl/charsets.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "terminal/draw.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"


#define realloc_document_links(doc, size) \
	ALIGN_LINK(&(doc)->links, (doc)->nlinks, size)

static struct screen_char *
realloc_line(struct document *document, int y, int x)
{
	struct line *line = realloc_lines(document, y);

	if (!line) return NULL;

	if (line->length <= x) {
		if (!ALIGN_LINE(&line->chars, line->length, x + 1))
			return NULL;

		line->length = x + 1;
	}

	return line->chars;
}

static inline struct link *
add_document_link(struct document *document, unsigned char *uri, int length,
		  int x, int y)
{
	struct link *link;
	struct point *point;

	if (!realloc_document_links(document, document->nlinks + 1))
		return NULL;

	link = &document->links[document->nlinks];

	if (!realloc_points(link, length))
		return NULL;

	link->n = length;
	link->type = LINK_HYPERTEXT;
	link->where = uri;
	link->color.background = document->options.default_bg;
	link->color.foreground = document->options.default_vlink;

	for (point = link->pos; length > 0; length--, point++, x++) {
		point->x = x;
		point->y = y;
	}

	document->nlinks++;

	return link;
}

/* Searches a word to find an email adress or an URI to add as a link. */
static inline int
check_link_word(struct document *document, unsigned char *uri, int length,
		int x, int y)
{
	struct uri test;
	unsigned char *where = NULL;
	unsigned char *mailto = memchr(uri, '@', length);
	int keep = uri[length];

	assert(document);
	if_assert_failed return 0;

	uri[length] = 0;

	if (mailto && mailto - uri < length) {
		where = straconcat("mailto:", uri, NULL);

	} else if (parse_uri(&test, uri)
		   && test.protocol != PROTOCOL_UNKNOWN
		   && (test.datalen || test.hostlen)) {
		where = memacpy(uri, length);
	}

	uri[length] = keep;

	if (where && !add_document_link(document, where, length, x, y)) {
		mem_free(where);
	}

	return where ? length : 0;
}

#define url_char(c) (		\
		(c) > ' '	\
		&& (c) != '<'	\
		&& (c) != '>'	\
		&& (c) != '('	\
		&& (c) != ')'	\
		&& (c) != '\''	\
		&& (c) != '"')

static inline int
get_uri_length(unsigned char *line, int length)
{
	int uri_end = 0;

	while (uri_end < length
	       && url_char(line[uri_end]))
		uri_end++;

	for (; uri_end > 0; uri_end--) {
		if (line[uri_end - 1] != '.'
		    && line[uri_end - 1] != ',')
			break;
	}

	return uri_end;
}

static inline int
add_document_line(struct document *document, int lineno,
		  unsigned char *line, int width, struct screen_char *template,
		  struct conv_table *convert_table)
{
	struct screen_char *pos, *end;
	int line_pos, expanded = 0;

	for (line_pos = 0; line_pos < width; line_pos++) {
		unsigned char line_char = line[line_pos];

		if (line_char == ASCII_TAB) {
			int tab_width = 7 - ((line_pos + expanded) & 7);

			expanded += tab_width;

		} else if (line_char < ' ' || line_char == ASCII_ESC) {
			line[line_pos] = '.';

		} else 	if (document->options.plain_display_links
			    && isalpha(line_char) ) {
			unsigned char *start = &line[line_pos];
			int len = get_uri_length(start, width - line_pos);
			int x = line_pos + expanded;

			if (!len) continue;

			if (check_link_word(document, start, len, x, lineno))
				line_pos += len;
		}
	}

	line = convert_string(convert_table, line, width, CSM_DEFAULT);
	if (!line) return 0;

	width += expanded;

	pos = realloc_line(document, lineno, width);
	if (!pos) return 0;

	expanded = 0;
	for (line_pos = 0, end = pos + width; pos < end; pos++, line_pos++) {
		unsigned char line_char = line[line_pos];

		if (line_char == ASCII_TAB) {
			int tab_width = 7 - ((line_pos + expanded) & 7);

			template->data = ' ';
			expanded += tab_width;

			for (; tab_width; tab_width--, pos++)
				copy_screen_chars(pos, template, 1);
		} else {
			template->data = line_char;
			copy_screen_chars(pos, template, 1);
		}
	}

	mem_free(line);

	return width;
}

static void
add_document_lines(struct document *document, unsigned char *source, int length,
		   struct conv_table *convert_table)
{
	struct screen_char template;
	struct color_pair colors;
	int lineno;

	document->width = 0;

	/* Setup the style */
	colors.foreground = global_doc_opts->default_fg;
	colors.background = global_doc_opts->default_bg;

	template.attr = 0;
	template.data = ' ';
	set_term_color(&template, &colors, global_doc_opts->color_flags, global_doc_opts->color_mode);

	for (lineno = 0; length > 0; lineno++) {
		unsigned char *xsource;
		int width, added;
		int step = 0;

		/* End of line detection.
		 * We handle \r, \r\n and \n types here. */
		for (width = 0; width < length; width++) {
			if (source[width] == ASCII_CR)
				step++;
			if (source[width + step] == ASCII_LF)
				step++;
			if (step) break;
		}

		/* We will touch the supplied source, so better replicate it. */
		xsource = memacpy(source, width);
		if (!xsource) continue;

		added = add_document_line(document, lineno, xsource, width, &template,
					  convert_table);
		mem_free(xsource);

		if (added) {
			/* Add (search) nodes on a line by line basis */
			struct node *node = mem_alloc(sizeof(struct node));
			if (node) {
				node->x = 0;
				node->y = lineno;
				node->height = 1;
				node->width = added;
				add_to_list(document->nodes, node);
			}

			int_lower_bound(&document->width, added);
		}

		/* Skip end of line chars too. */
		width += step;
		length -= width;
		source += width;
	}
}

void
render_plain_document(struct cache_entry *ce, struct document *document)
{
	struct fragment *fr = ce->frag.next;
	struct conv_table *convert_table;
	struct string head;
	unsigned char *source = NULL;
	int length = 0;

	if (!init_string(&head)) return;

	if (!((void *)fr == &ce->frag || fr->offset || !fr->length)) {
		source = fr->data;
		length = fr->length;
	}

	if (ce->head) add_to_string(&head, ce->head);

	convert_table = get_convert_table(head.source, document->options.cp,
					  document->options.assume_cp,
					  &document->cp,
					  &document->cp_status,
					  document->options.hard_assume);

	done_string(&head);

	document->title = get_no_post_url(document->url, NULL);
	add_document_lines(document, source, length, convert_table);

	document->bgcolor = global_doc_opts->default_bg;
}
