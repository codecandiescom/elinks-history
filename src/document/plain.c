/* Plain text document renderer */
/* $Id: plain.c,v 1.4 2003/10/31 20:40:25 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"
#include "main.h"

#include "cache/cache.h"
#include "document/document.h"
#include "document/draw.h"
#include "document/html/renderer.h" /* TODO: Move get_convert_table() */
#include "terminal/draw.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"


/* TODO: Highlight uris in the plaintext (optional ofcourse) */

static void
add_document_lines(struct document *document, unsigned char *source)
{
	struct color_pair colors;
	int length = strlen(source);
	int lineno;

	document->width = 0;

	/* Setup the style */
	colors.foreground = d_opt->default_fg;
	colors.background = d_opt->default_bg;

	for (lineno = 0; length > 0; lineno++) {
		unsigned char *lineend = strchr(source, '\n');
		int width = lineend ? lineend - source: strlen(source);
		struct node *node = mem_alloc(sizeof(struct node));
		struct screen_char *pos, *end;

		/* Add (search) nodes on a line by line basis */
		if (node) {
			node->x = 0;
			node->y = lineno;
			node->height = 1;
			node->width = width;
			add_to_list(document->nodes, node);
		}

		pos = get_document_line(document, lineno, width, &colors);
		if (!pos) continue;

		for (end = pos + width; pos < end; pos++, source++) {
			pos->data = (*source < ' ' || *source == ASCII_ESC)
				  ? ' ' : *source;
		}

		/* Skip the newline too. */
		length -= width + 1;
		source += 1;
	}
}

void
render_plaintext_document(struct document *document, struct cache_entry *ce)
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

	add_document_lines(document, source);
	document->bgcolor = d_opt->default_bg;

	mem_free(source);
}
