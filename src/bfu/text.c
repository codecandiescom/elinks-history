/* Text widget implementation. */
/* $Id: text.c,v 1.29 2003/11/06 23:38:27 jonas Exp $ */

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

static inline int
split_line(unsigned char *text, int max_length, int text_length)
{
	int length = int_min(max_length, text_length);
	/* Always prefer to split at new lines */
	unsigned char *split = memchr(text, '\n', length + 1);

	if (!split) {
		/* If the length is the rest of the text split there */
		if (length == text_length) return length;

		/* Else find a good place to split starting from last char */
		split = text + length;

		while (*split != ' ' && split > text)
			split--;

		if (split <= text) return length;
	}

	return split - text;
}

/* Format text according to dialog dimensions and alignment. */
void
dlg_format_text(struct terminal *term, unsigned char *text,
		int x, int *y, int dlg_width, int *real_width,
		struct color_pair *color, enum format_align align)
{
	int length = strlen(text);
	int line_y = *y;
	int split;

	/* Layout the text line by line working on text chunks of max
	 * @dlg_width chars. */
	for (; length > 0; line_y++, text += split, length -= split) {
		int shift;

		/* Skip any leading space from last line split */
		if (*text == ' ' || *text == '\n') {
			text++;
			length--;
		}

		split = int_max(split_line(text, dlg_width, length), 1);
		if (real_width) int_lower_bound(real_width, split);

		assert(split <= dlg_width);

		if (!term) continue;

		/* Calculate the number of chars to indent */
		if (align == AL_CENTER)
			shift = (dlg_width - split) / 2;
		else if (align == AL_RIGHT)
			shift = dlg_width - split;
		else
			shift = 0;

		draw_text(term, x + shift, line_y, text, split, 0, color);
	}

	*y = line_y;
}
