/* Text widget implementation. */
/* $Id: text.c,v 1.37 2003/11/07 16:10:55 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/style.h"
#include "bfu/text.h"
#include "intl/gettext/libintl.h"
#include "terminal/draw.h"
#include "terminal/terminal.h"
#include "util/color.h"

void
text_width(struct terminal *term, register unsigned char *text,
	   int *minwidth, int *maxwidth)
{
	do {
		register int cmax = 0;
		register int cmin = 0;
		int cmin_stop = 0;

		while (*text && *text != '\n') {
			if (!cmin_stop) {
				if (*text == ' ')
					cmin_stop = 1;
				cmin++;
			}
			cmax++;
			text++;
		}

		if (maxwidth && cmax > *maxwidth) *maxwidth = cmax;
		if (minwidth && cmin > *minwidth) *minwidth = cmin;

	} while (*(text++));
}

#define is_unsplitable(pos) (*(pos) && *(pos) != '\n' && *(pos) != ' ')

static inline int
split_line(unsigned char *text, int w, int x)
{
	unsigned char *tx;
	unsigned char *split = text;
	int line_x = x;
	int line_width;

	do {
		while (is_unsplitable(split)) {
			split++;
			line_x++;
		}

		tx = ++split;
		line_width = line_x - x;
		line_x++;
		if (*(split - 1) != ' ') break;

		while (is_unsplitable(tx))
			tx++;
	} while (tx - split < w - line_width);

	assertm(split - text == line_x - x);
	assertm(line_width + 1 == line_x - x);

	return line_width;
}

/* Format text according to dialog dimensions and alignment. */
/* TODO: Longer names for local variables. */
void
dlg_format_text(struct terminal *term,
		unsigned char *text, int x, int *y, int w, int *rw,
		struct color_pair *color, enum format_align align)
{
	int line_width;

	for (; *text; text += line_width, (*y)++) {
		int shift;

		/* Skip any leading space from last line split */
		if (*text == ' ' || *text == '\n') text++;

		line_width = split_line(text, w, x);
		shift = (align == AL_CENTER ? int_max((w - line_width) / 2, 0) : 0);

		assert(line_width - x <= w && shift < w);

		if (term && line_width) {
			draw_text(term, x + shift, *y, text, line_width, 0, color);
		}

		if (rw) int_lower_bound(rw, line_width);
	}
}
