/* Text widget implementation. */
/* $Id: text.c,v 1.46 2003/11/07 18:32:30 jonas Exp $ */

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
split_line(unsigned char *text, int max_width)
{
	unsigned char *split = text;

	while (*split && *split != '\n') {
		unsigned char *next_split = split + 1;

		while (is_unsplitable(next_split))
			next_split++;

		if (next_split - text > max_width) {
			/* Force a split if no position was found yet */
			if (split == text) return max_width;
			break;
		}

		split = next_split;
	}

	return split - text;
}

/* Format text according to dialog dimensions and alignment. */
void
dlg_format_text(struct terminal *term, unsigned char *text,
		int x, int *y, int dlg_width, int *real_width,
		struct color_pair *color, enum format_align align)
{
	int line_width;

	for (; *text; text += line_width, (*y)++) {
		int shift;

		/* Skip any leading space from last line split */
		if (*text == ' ' || *text == '\n') text++;

		line_width = split_line(text, dlg_width);

		if (real_width) int_lower_bound(real_width, line_width);
		if (!term || !line_width) continue;

		/* Calculate the number of chars to indent */
		if (align == AL_CENTER)
			shift = (dlg_width - line_width) / 2;
		else if (align == AL_RIGHT)
			shift = dlg_width - line_width;
		else
			shift = 0;

		assert(line_width - x <= dlg_width && shift < dlg_width);

		draw_text(term, x + shift, *y, text, line_width, 0, color);
	}
}
