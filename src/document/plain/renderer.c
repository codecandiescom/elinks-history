/* Plain text document renderer */
/* $Id: renderer.c,v 1.112 2004/08/16 10:14:23 miciah Exp $ */

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
#include "document/plain/renderer.h"
#include "document/renderer.h"
#include "intl/charsets.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "terminal/draw.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"


struct plain_renderer {
	/* The document being renderered */
	struct document *document;

	/* The data and data length of the defragmented cache entry */
	unsigned char *source;
	int length;

	/* The convert table that should be used for converting line strings to
	 * the rendered strings. */
	struct conv_table *convert_table;

	/* The default template char data for text */
	struct screen_char template;

	/* The maximum width any line can have (used for wrapping text) */
	int max_width;

	/* The current line number */
	int lineno;

	/* Are we doing line compression */
	unsigned int compress:1;
};

#define realloc_document_links(doc, size) \
	ALIGN_LINK(&(doc)->links, (doc)->nlinks, size)

static struct screen_char *
realloc_line(struct document *document, int x, int y)
{
	struct line *line = realloc_lines(document, y);

	if (!line) return NULL;

	if (x > line->length) {
		if (!ALIGN_LINE(&line->chars, line->length, x))
			return NULL;

		line->length = x;
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

	link->npoints = length;
	link->type = LINK_HYPERTEXT;
	link->where = uri;
	link->color.background = document->options.default_bg;
	link->color.foreground = document->options.default_vlink;

	for (point = link->points; length > 0; length--, point++, x++) {
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

	} else if (parse_uri(&test, uri) == URI_ERRNO_OK
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
		&& !isquote(c))

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
add_document_line(struct plain_renderer *renderer,
		  unsigned char *line, int line_width)
{
	struct document *document = renderer->document;
	struct screen_char *template = &renderer->template;
	enum screen_char_attr saved_renderer_templated_attr = template->attr;
	struct screen_char *pos;
	int lineno = renderer->lineno;
	int expanded = 0;
	int width = line_width;
	int line_pos;
	int backspaces = 0;

	line = convert_string(renderer->convert_table, line, width, CSM_NONE, &width);
	if (!line) return 0;

	/* Now expand tabs and handle urls if needed.
	 * Here little code redundancy to improve performance. */
	if (document->options.plain_display_links) {
		int was_alpha_char = 1; /* to match start of line too. */

		for (line_pos = 0; line_pos < width; line_pos++) {
			unsigned char line_char = line[line_pos];

			if (line_char == ASCII_TAB) {
				int tab_width = 7 - ((line_pos + expanded) & 7);

				expanded += tab_width;
				was_alpha_char = 0;

			} else if (line_char == ASCII_BS) {
				if (backspaces * 2 < line_pos) {
					backspaces++;
					expanded -= 2;
					was_alpha_char = 0;
				} else {
					expanded--;
				}

			} else {
				/* We only want to detect url if there is at least
				 * to consecutive alphanumeric characters, or when
				 * we are at the very start of line.
				 * It improves performance a bit. --Zas */
				int is_alpha_char = isalpha(line_char);

				if (is_alpha_char && was_alpha_char) {
					int pos = int_max(0, line_pos - 1);
					unsigned char *start = &line[pos];
					int len = get_uri_length(start, width - pos);
					int x = pos + expanded;

					if (len
					    && check_link_word(document, start, len, x, lineno))
						line_pos += len - 2;

					was_alpha_char = 1;
				} else {
					was_alpha_char = is_alpha_char;
				}
			}
		}
	} else {
		for (line_pos = 0; line_pos < width; line_pos++) {
			unsigned char line_char = line[line_pos];

			if (line_char == ASCII_TAB) {
				int tab_width = 7 - ((line_pos + expanded) & 7);

				expanded += tab_width;
			} else if (line_char == ASCII_BS) {
				if (backspaces * 2 < line_pos) {
					backspaces++;
					expanded -= 2;
				} else {
					expanded--;
				}
			}
		}
	}

	pos = realloc_line(document, width + expanded, lineno);
	if (!pos) {
		mem_free(line);
		return 0;
	}

	expanded = backspaces = 0;
	for (line_pos = 0; line_pos < width; line_pos++) {
		unsigned char line_char = line[line_pos];

		if (line_char == ASCII_TAB) {
			int tab_width = 7 - ((line_pos + expanded) & 7);

			expanded += tab_width;

			template->data = ' ';
			do
				copy_screen_chars(pos++, template, 1);
			while (tab_width--);

			template->attr = saved_renderer_templated_attr;

		} else if (line_char == ASCII_BS) {
			if (backspaces * 2 >= line_pos) {
				/* We've backspaced to the start
				 * of the line */
				expanded--; /* Don't count it */
				continue;
			}

			pos--;  /* Backspace */

			/* Handle x^H_ as _^Hx */
			if (line[line_pos + 1] == '_'
			    && line[line_pos - 1] != '_') /* No inf. loop
							   * swapping two
							   * underscores */
			{
				unsigned char saved_char = line[line_pos - 1];

				/* x^H_ becomes _^Hx */
				line[line_pos - 1] = line[line_pos + 1];
				line[line_pos + 1] = saved_char;

				/* Go back and reparse the swapped characters */
				line_pos -= 2;
				backspaces--;
				continue;
			}

			expanded -= 2; /* Don't count the backspace character 
				        * or the deleted character
				        * when returning the line's width
				        * or when expanding tabs */

			backspaces++;

			if (pos->data == '_' && line[line_pos + 1] == '_') {
				/* Is _^H_ an underlined underscore
				 * or an emboldened underscore? */

				if (backspaces * 2 < line_pos
				    && (pos - 1)->attr) {
					/* There is some preceding text,
					 * and it has an attribute; copy it */
					template->attr |= (pos - 1)->attr;
				} else {
					/* Default to bold; seems more useful
					 * than underlining the underscore */
					template->attr |= SCREEN_ATTR_BOLD;
				}

			} else if (pos->data == '_') {
				/* Underline _^Hx */

				template->attr |= SCREEN_ATTR_UNDERLINE;

			} else if (pos->data == line[line_pos + 1]) {
				/* Embolden x^Hx */

				template->attr |= SCREEN_ATTR_BOLD;
			}

			/* Handle _^Hx^Hx as both bold and underlined */
			if (template->attr)
				template->attr |= pos->attr;
		} else {
			if (!isscreensafe(line_char))
				line_char = '.';
			template->data = line_char;
			copy_screen_chars(pos++, template, 1);

			template->attr = saved_renderer_templated_attr;
		}

		/* Detect copy of nul chars to screen, this should not occur.
		 * --Zas */
		assert(pos[-1].data);
	}

	mem_free(line);

	return width + expanded;
}

static void
init_template(struct screen_char *template, color_t background, color_t foreground)
{
	struct color_pair colors = INIT_COLOR_PAIR(background, foreground);

	template->attr = 0;
	template->data = ' ';
	set_term_color(template, &colors, global_doc_opts->color_flags, global_doc_opts->color_mode);
}

static struct node *
add_node(struct plain_renderer *renderer, int x, int width, int height)
{
	struct node *node = mem_alloc(sizeof(struct node));

	if (node) {
		struct document *document = renderer->document;

		set_box(&node->box, x, renderer->lineno, width, height);

		int_lower_bound(&document->width, width);
		int_lower_bound(&document->height, height);

		add_to_list(document->nodes, node);
	}

	return node;
}

static void
add_document_lines(struct plain_renderer *renderer)
{
	unsigned char *source = renderer->source;
	int length = renderer->length;
	int was_empty_line = 0;

	for (; length > 0; renderer->lineno++) {
		unsigned char *xsource;
		int width, added, only_spaces = 1, spaces = 0, was_spaces = 0;
		int last_space = 0;
		int step = 0;
		int doc_width = int_min(renderer->max_width, length);

		/* End of line detection.
		 * We handle \r, \r\n and \n types here. */
		for (width = 0; width < doc_width; width++) {
			if (source[width] == ASCII_CR)
				step++;
			if (source[width + step] == ASCII_LF)
				step++;
			if (step) break;

			if (isspace(source[width])) {
				last_space = width;
				if (only_spaces)
					spaces++;
				else
					was_spaces++;
			} else {
				only_spaces = 0;
				was_spaces = 0;
			}
		}

		if (only_spaces && step) {
			if (renderer->compress && was_empty_line) {
				/* Successive empty lines
				 * will appear as one. */
				length -= step + spaces;
				source += step + spaces;
				renderer->lineno--;
				continue;
			}
			was_empty_line = 1;

			/* No need to keep whitespaces
			* on an empty line. */
			source += spaces;
			length -= spaces;
			width -= spaces;

		} else {
			was_empty_line = 0;

			if (was_spaces && step) {
				/* Drop trailing whitespaces. */
				width -= was_spaces;
				step += was_spaces;
			}
			if (!step && (width < length) && last_space) {
				width = last_space;
				step = 1;
			}
		}

		assert(width >= 0);

		/* We will touch the supplied source, so better replicate it. */
		xsource = memacpy(source, width);
		if (!xsource) continue;

		added = add_document_line(renderer, xsource, width);
		mem_free(xsource);

		if (added) {
			/* Add (search) nodes on a line by line basis */
			add_node(renderer, 0, added, 1);
		}

		/* Skip end of line chars too. */
		width += step;
		length -= width;
		source += width;
	}

	assert(!length);
}

void
render_plain_document(struct cache_entry *cached, struct document *document,
		      struct string *buffer)
{
	struct conv_table *convert_table;
	unsigned char *head = empty_string_or_(cached->head);
	struct plain_renderer renderer;

	assert(!list_empty(cached->frag));

	convert_table = get_convert_table(head, document->options.cp,
					  document->options.assume_cp,
					  &document->cp,
					  &document->cp_status,
					  document->options.hard_assume);

	document->bgcolor = global_doc_opts->default_bg;
	document->width = 0;

	renderer.source = buffer->source;
	renderer.length = buffer->length;

	renderer.document = document;
	renderer.lineno = 0;
	renderer.convert_table = convert_table;
	renderer.compress = document->options.plain_compress_empty_lines;
	renderer.max_width = document->options.wrap ? document->options.box.width
						    : INT_MAX;

	/* Setup the style */
	init_template(&renderer.template, global_doc_opts->default_bg,
					  global_doc_opts->default_fg);

	add_document_lines(&renderer);
}
