/* Text widget implementation. */
/* $Id: text.c,v 1.66 2003/12/03 17:16:08 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "bfu/style.h"
#include "bfu/text.h"
#include "config/kbdbind.h"
#include "intl/gettext/libintl.h"
#include "terminal/draw.h"
#include "terminal/kbd.h"
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

#define LINES_GRANULARITY 0x7
#define realloc_lines(x, o, n) mem_align_alloc(x, o, n, sizeof(unsigned char *), LINES_GRANULARITY)

/* Find the start of each line with the current max width */
static unsigned char **
split_lines(struct widget_data *widget_data, int max_width)
{
	unsigned char *text = widget_data->widget->text;
	unsigned char **lines = (unsigned char **) widget_data->cdata;
	int line = 0;
	int width;

	if (widget_data->info.text.max_width == max_width) return lines;

	/* We want to recalculate the max line width */
	widget_data->w = 0;

	for (; *text; text += width) {

		/* Skip any leading space from last line split */
		if (*text == ' ' || *text == '\n') text++;

		width = split_line(text, max_width);
		int_lower_bound(&widget_data->w, width);

		if (!realloc_lines(&lines, line, line + 1))
			break;

		lines[line++]= text;
	}

	/* Yes it might be a bit ugly on the other hand it will be autofreed
	 * for us. */
	widget_data->cdata = (unsigned char *) lines;
	widget_data->info.text.lines = line;
	widget_data->info.text.max_width = max_width;

	return lines;
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
dlg_format_text(struct terminal *term, struct widget_data *widget_data,
		int x, int *y, int dlg_width, int *real_width, int max_height)
{
	unsigned char *text = widget_data->widget->text;
	unsigned char saved = 0;
	unsigned char *saved_pos = NULL;

	/* If we are drawing set up the dimensions before setting up the
	 * scrolling. */
	widget_data->x = x;
	widget_data->y = *y;
	widget_data->h = max_height - 3;
	if (widget_data->h < 0) {
		widget_data->h = 0;
		return;
	}

	/* Always reset @current if we do not need to scroll */
	if (widget_data->h >= widget_data->info.text.lines)
		widget_data->info.text.current = 0;

	/* Can we scroll and do we even have to? */
	if (widget_data->widget->info.text.is_scrollable
	    && (widget_data->info.text.max_width != dlg_width
		|| widget_data->h < widget_data->info.text.lines)) {
		unsigned char **lines;
		int current;
		int visible;

		/* Ensure that the current split is valid but don't
		 * split if we don't have to */
		if (widget_data->w != dlg_width
		    && !split_lines(widget_data, dlg_width))
			return;

		lines = (unsigned char **) widget_data->cdata;

		/* Make maximum number of lines available */
		visible = int_max(widget_data->info.text.lines - widget_data->h,
				  widget_data->h);

		int_bounds(&widget_data->info.text.current, 0, visible);
		current = widget_data->info.text.current;

		/* Set the current position */
		text = lines[current];

		/* Do we have to force a text end */
		visible = widget_data->info.text.lines - current;
		if (visible > widget_data->h) {
			int lines_pos = current + widget_data->h;

			saved_pos = lines[lines_pos];

			/* We save the start of lines so backtrack to see
			 * if the previous line has a line end that should
			 * also be trimmed. */
			if (lines_pos > 0 && saved_pos[-1] == '\n')
				saved_pos--;

			saved = *saved_pos;
			*saved_pos = 0;
		}

		/* Force dialog to be the width of the longest line */
		if (real_width) int_lower_bound(real_width, widget_data->w);

	}

	dlg_format_text_do(term, text,
		x, y, dlg_width, real_width,
		term ? get_bfu_color(term, "dialog.text") : NULL,
		widget_data->widget->info.text.align);

	if (widget_data->widget->info.text.is_label) (*y)--;
	widget_data->h = (*y) - widget_data->y;

	/* If we scrolled and something was trimmed restore it */
	if (saved && saved_pos) *saved_pos = saved;
}

/* TODO: Some kind of scroll bar or scroll percentage */

static void
display_text(struct widget_data *widget_data, struct dialog_data *dlg_data, int sel)
{
	struct window *win = dlg_data->win;
	int x = dlg_data->x + dlg_data->width - DIALOG_LEFT_BORDER - 1;
	int y = widget_data->y;
	int height = widget_data->h;
	int scale, current, step;

	if (!text_is_scrollable(widget_data) || height <= 0) return;

	draw_area(win->term, x, y, 1, height, ' ', 0,
		    get_bfu_color(win->term, "dialog.scrollbar"));

	current = sel ? widget_data->info.text.current : 0;
	scale = height * 100 / widget_data->info.text.lines;

	/* Scale the offset of @current */
	step = (current + 1) * scale / 100;
	int_bounds(&step, 0, widget_data->h - 1);
	y += step;

	/* Scale the number of visible lines */
	height = int_max(height * scale / 100, 1);
	int_bounds(&height, 1, int_max(widget_data->h - step, 1));

	draw_area(win->term, x, y, 1, height, ' ', 0,
		  get_bfu_color(win->term, "dialog.scrollbar-selected"));

	/* Hope this is at least a bit reasonable. Set blockable cursor and
	 * window pointer to start of the first text line. */
	set_cursor(win->term, widget_data->x, widget_data->y, 1);
	set_window_ptr(win, widget_data->x, widget_data->y);
}

static void
format_and_display_text(struct widget_data *widget_data,
			struct dialog_data *dlg_data,
			int current)
{
	struct terminal *term = dlg_data->win->term;
	int y = widget_data->y;
	int height = dialog_max_height(term);
	int lines = widget_data->info.text.lines;

	int_bounds(&current, 0, lines - widget_data->h);

	if (widget_data->info.text.current == current) return;

	widget_data->info.text.current = current;

	draw_area(term, widget_data->x, widget_data->y,
		  widget_data->w, widget_data->h, ' ', 0,
		  get_bfu_color(term, "dialog.generic"));

	dlg_format_text(term, widget_data,
			widget_data->x, &y, widget_data->w, NULL,
			height);

	display_text(widget_data, dlg_data, 1);
	redraw_from_window(dlg_data->win);
}

static int
kbd_text(struct widget_data *widget_data, struct dialog_data *dlg_data,
	 struct term_event *ev)
{
	int current = widget_data->info.text.current;

	switch (kbd_action(KM_MAIN, ev, NULL)) {
		case ACT_UP:
		case ACT_SCROLL_UP:
			current--;
			break;

		case ACT_DOWN:
		case ACT_SCROLL_DOWN:
			current++;
			break;

		case ACT_PAGE_UP:
			current -= widget_data->h;
			break;

		case ACT_PAGE_DOWN:
			current += widget_data->h;
			break;

		case ACT_HOME:
			current = 0;
			break;

		case ACT_END:
			current = widget_data->info.text.lines;
			break;

		default:
			return EVENT_NOT_PROCESSED;
	}

	format_and_display_text(widget_data, dlg_data, current);

	return EVENT_PROCESSED;
}

static int
mouse_text(struct widget_data *widget_data, struct dialog_data *dlg_data,
	   struct term_event *ev)
{
	int x = dlg_data->x + dlg_data->width - DIALOG_LEFT_BORDER - 1;
	int y = widget_data->y;
	int height = widget_data->h;
	int current = widget_data->info.text.current;

	if ((ev->b & BM_BUTT) >= B_WHEEL_UP
	    || ev->x != x
	    || ev->y < y || ev->y >= y + height)
		return EVENT_NOT_PROCESSED;

	/* If in upper half, scroll up else scroll down. */
	if (ev->y < y + height/2)
		current--;
	else
		current++;

	format_and_display_text(widget_data, dlg_data, current);
	return EVENT_PROCESSED;
}


struct widget_ops text_ops = {
	display_text,
	NULL,
	mouse_text,
	kbd_text,
	NULL,
};
