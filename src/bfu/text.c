/* Text widget implementation. */
/* $Id: text.c,v 1.5 2003/01/03 00:02:26 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/align.h"
#include "bfu/text.h"
#include "intl/language.h"
#include "lowlevel/terminal.h"


void
max_text_width(struct terminal *term, unsigned char *text, int *width)
{
	text = _(text, term);
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
	text = _(text, term);
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
void
dlg_format_text(struct terminal *term, struct terminal *t2,
		unsigned char *text, int x, int *y, int w, int *rw,
		int co, enum format_align align)
{
	text = _(text, t2);
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

		s = (align & AL_MASK) == AL_CENTER ? (w - ww) / 2 : 0;

		if (s < 0) s = 0;

		while (tt < text - 1) {
			if (s >= w) {
				s = 0;
			   	(*y)++;
				if (rw) *rw = w;
				rw = NULL;
			}
			if (term) set_char(term, x + s, *y, co | *tt);
			s++;
			tt++;
		}
		if (rw && ww > *rw) *rw = ww;
		(*y)++;
	} while (*(text - 1));
}
