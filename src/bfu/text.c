/* Text widget implementation. */
/* $Id: text.c,v 1.23 2003/11/06 01:59:24 jonas Exp $ */

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

/* Format text according to dialog dimensions and alignment. */
/* TODO: Longer names for local variables. */
void
dlg_format_text(struct terminal *term,
		unsigned char *text, int x, int *y, int w, int *rw,
		struct color_pair *color, enum format_align align)
{
	do {
		unsigned char *tx;
		unsigned char *split = text;
		int shift;
		int line_x = x;
		int line_width;

		do {
			while (*split && *split != '\n' && *split != ' ') {
				split++;
			       	line_x++;
			}

			tx = ++split;
			line_width = line_x - x;
			line_x++;
			if (*(split - 1) != ' ') break;

			while (*tx && *tx != '\n' && *tx != ' ')
				tx++;
		} while (tx - split < w - line_width);

		shift = (align == AL_CENTER ? int_max((w - line_width) / 2, 0) : 0);

		if (shift >= w) {
			shift = 0;
			if (rw) {
				*rw = w;
				rw = NULL;
			}
		}

		if (term && split > text) {
			int length = split - text - 1;

			draw_text(term, x + shift, *y, text, length, 0, color);
		}

		if (rw) int_lower_bound(rw, line_width);
		text = split;
		(*y)++;
	} while (*(text - 1));
}
