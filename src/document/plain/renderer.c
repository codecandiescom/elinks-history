/* Plain text document renderer */
/* $Id: renderer.c,v 1.20 2003/11/14 13:27:09 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <string.h>

#include "elinks.h"
#include "main.h"

#include "cache/cache.h"
#include "document/document.h"
#include "document/html/renderer.h" /* TODO: Move get_convert_table() */
#include "document/plain/renderer.h"
#include "protocol/uri.h"
#include "terminal/draw.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"


#define LINES_GRANULARITY	0x7F
#define LINE_GRANULARITY	0x0F
#define LINK_GRANULARITY	0x7F

#define ALIGN_LINES(x, o, n) mem_align_alloc(x, o, n, sizeof(struct line), LINES_GRANULARITY)
#define ALIGN_LINE(x, o, n) mem_align_alloc(x, o, n, sizeof(struct screen_char), LINE_GRANULARITY)

#define realloc_document_links(doc, size) \
	mem_align_alloc(&(doc)->links, (doc)->nlinks, size, sizeof(struct link), LINK_GRANULARITY)

#define realloc_points(link, size) \
	mem_align_alloc(&(link)->pos, (link)->n, size, sizeof(struct point), 0)

static struct line *
realloc_lines(struct document *document, int y)
{
	assert(document);
	if_assert_failed return 0;

	if (document->height <= y) {
		if (!ALIGN_LINES(&document->data, document->height, y + 1))
			return NULL;

		document->height = y + 1;
	}

	return &document->data[y];
}

static struct screen_char *
realloc_line(struct document *document, int y, int x)
{
	struct line *line = realloc_lines(document, y);

	if (!line) return NULL;

	if (line->l <= x) {
		if (!ALIGN_LINE(&line->d, line->l, x + 1))
			return NULL;

		line->l = x + 1;
	}

	return line->d;
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

	} else if (parse_uri(&test, uri) && (test.datalen || test.hostlen)) {
		where = memacpy(uri, length);
	}

	uri[length] = keep;

	if (where && !add_document_link(document, where, length, x, y)) {
		mem_free(where);
	}

	return where ? length : 0;
}

static inline int
get_uri_length(unsigned char *line, int length)
{
	int uri_end = 0;

	while (uri_end < length && !isspace(line[uri_end])
	       && line[uri_end] != '"'
	       && line[uri_end] != '('
	       && line[uri_end] != '<'
	       && line[uri_end] != '>')
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
		  unsigned char *line, int width, struct screen_char *template)
{
	struct screen_char *pos, *end;
	int line_pos, expanded = 0;

	for (line_pos = 0; line_pos < width; line_pos++) {
		unsigned char line_char = line[line_pos];

		if (line_char == ASCII_TAB) {
			int tab_width = 7 - ((line_pos + expanded) & 7);

			expanded += tab_width;
			continue;

		} else if (line_char < ' ' || line_char == ASCII_ESC) {
			line[line_pos] = ' ';
			continue;

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

	return width;
}

static void
add_document_lines(struct document *document, unsigned char *source)
{
	struct screen_char template;
	struct color_pair colors;
	int length = strlen(source);
	int lineno;

	document->width = 0;

	/* Setup the style */
	colors.foreground = global_doc_opts->default_fg;
	colors.background = global_doc_opts->default_bg;

	template.attr = 0;
	template.data = ' ';
	set_term_color(&template, &colors, global_doc_opts->color_flags, global_doc_opts->color_mode);

	for (lineno = 0; length > 0; lineno++) {
		unsigned char *lineend = strchr(source, '\n');
		int width = lineend ? lineend - source: strlen(source);
		int added;

		added = add_document_line(document, lineno, source, width, &template);

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

		/* Skip the newline too. */
		length -= width + 1;
		source += width + 1;
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
		length= fr->length;
	}

	if (ce->head) add_to_string(&head, ce->head);

	convert_table = get_convert_table(head.source, document->options.cp,
					  document->options.assume_cp,
					  &document->cp,
					  &document->cp_status,
					  document->options.hard_assume);

	done_string(&head);

	source = convert_string(convert_table, source, length, CSM_DEFAULT);
	if (!source) return;

	document->title = stracpy(document->url);
	add_document_lines(document, source);

	document->bgcolor = global_doc_opts->default_bg;

	mem_free(source);
}
