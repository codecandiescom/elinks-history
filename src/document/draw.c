/* Document drawing library */
/* $Id: draw.c,v 1.1 2003/10/31 19:54:09 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"
#include "main.h"

#include "document/document.h"
#include "terminal/draw.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"


#define LINES_GRANULARITY	0x7F
#define LINE_GRANULARITY	0x0F

#define ALIGN_LINES(x, o, n) mem_align_alloc(x, o, n, sizeof(struct line), LINES_GRANULARITY)
#define ALIGN_LINE(x, o, n) mem_align_alloc(x, o, n, sizeof(struct screen_char), LINE_GRANULARITY)

static inline struct line *
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

struct screen_char *
get_document_line(struct document *document, int y, int x)
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
