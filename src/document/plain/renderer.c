/* Plain text document renderer */
/* $Id: renderer.c,v 1.6 2003/11/13 14:28:08 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"
#include "main.h"

#include "cache/cache.h"
#include "document/document.h"
#include "document/html/renderer.h" /* TODO: Move get_convert_table() */
#include "document/plain/renderer.h"
#include "terminal/draw.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"


/* TODO: Highlight uris in the plaintext (optional ofcourse) */

#define LINES_GRANULARITY	0x7F
#define LINE_GRANULARITY	0x0F

#define ALIGN_LINES(x, o, n) mem_align_alloc(x, o, n, sizeof(struct line), LINES_GRANULARITY)
#define ALIGN_LINE(x, o, n) mem_align_alloc(x, o, n, sizeof(struct screen_char), LINE_GRANULARITY)

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
		} else if (line_char < ' ' || line_char == ASCII_ESC) {
			line[line_pos] = ' ';
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
