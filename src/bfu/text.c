/* Text widget implementation. */
/* $Id: text.c,v 1.17 2003/11/04 22:01:27 zas Exp $ */

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
/* TODO: Optimized version tha wont decode the color for every draw_char(). */
void
dlg_format_text(struct terminal *term, struct terminal *t2,
		unsigned char *text, int x, int *y, int w, int *rw,
		struct color_pair *color, enum format_align align)
{
	do {
		unsigned char *tx;
		unsigned char *tt = text;
		int s;
		int xx = x;
		int ww;

		do {
			while (*text && *text != '\n' && *text != ' ') {
#if 0
				if (term)
					set_char(term, xx, *y, co | *text);
#endif
				text++;
			       	xx++;
			}

			tx = ++text;
			ww = xx - x;
			xx++;
			if (*(text - 1) != ' ') break;

			while (*tx && *tx != '\n' && *tx != ' ')
				tx++;
		} while (tx - text < w - ww);

		s = align == AL_CENTER ? (w - ww) / 2 : 0;

		if (s < 0) s = 0;

		while (tt < text - 1) {
			if (s >= w) {
				s = 0;
			   	(*y)++;
				if (rw) *rw = w;
				rw = NULL;
			}
			if (term) draw_char(term, x + s, *y, *tt, 0, color);
			s++;
			tt++;
		}
		if (rw && ww > *rw) *rw = ww;
		(*y)++;
	} while (*(text - 1));
}
