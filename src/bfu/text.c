/* Text widget implementation. */
/* $Id: text.c,v 1.51 2003/11/09 15:05:17 pasky Exp $ */

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
dlg_format_text_do(struct terminal *term, unsigned char *text,
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

		assert(line_width <= dlg_width && shift < dlg_width);

		draw_text(term, x + shift, *y, text, line_width, 0, color);
	}
}

void
layout_text_widget(struct terminal *term, struct widget_data *widget_data,
		   int x, int *y, int dlg_width, int *real_width)
{
	dlg_format_text_do(term, widget_data->widget->text,
			x, y, dlg_width, real_width,
			term ? get_bfu_color(term, "dialog.text") : NULL,
			widget_data->widget->info.text.align);
}

struct widget_ops text_ops = {
	NULL,
	NULL,
	NULL,
	NULL,
	NULL,
};
