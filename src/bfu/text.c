/* Text widget implementation. */
/* $Id: text.c,v 1.16 2003/08/23 16:44:42 jonas Exp $ */

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

		if (cmax > *maxwidth) *maxwidth = cmax;
		if (cmin > *minwidth) *minwidth = cmin;

	} while (*(text++));
}

void
max_text_width(struct terminal *term, unsigned char *text, int *width)
{
	do {
		int c = 0;

		while (*text && *text != '\n') {
			text++;
			c++;
		}
		if (c > *width) *width = c;
	} while (*(text++));
}

void
min_text_width(struct terminal *term, unsigned char *text, int *width)
{
	do {
		int c = 0;

		while (*text && *text != '\n' && *text != ' ') {
			text++;
			c++;
		}
		if (c > *width) *width = c;
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
