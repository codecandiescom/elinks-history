/* Text widget implementation. */
/* $Id: text.c,v 1.21 2003/11/06 00:32:48 jonas Exp $ */

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
		int xx = x;
		int ww;

		do {
			while (*split && *split != '\n' && *split != ' ') {
				split++;
			       	xx++;
			}

			tx = ++split;
			ww = xx - x;
			xx++;
			if (*(split - 1) != ' ') break;

			while (*tx && *tx != '\n' && *tx != ' ')
				tx++;
		} while (tx - split < w - ww);

		shift = (align == AL_CENTER ? int_max((w - ww) / 2, 0) : 0);

		if (shift >= w) {
			shift = 0;
			(*y)++;
			if (rw) {
				*rw = w;
				rw = NULL;
			}
		}

		if (term) {
			int length = split - text;

			draw_text(term, x + shift, *y, text, length, 0, color);
		}

		if (rw) int_lower_bound(rw, ww);
		text = split;
		(*y)++;
	} while (*(text - 1));
}
