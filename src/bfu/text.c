/* Text widget implementation. */
/* $Id: text.c,v 1.84 2004/01/30 19:01:45 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
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

#define is_unsplitable(pos) (*(pos) && *(pos) != '\n' && !isspace(*(pos)))

/* Returns length of substring (from start of @text) before a split. */
static inline int
split_line(unsigned char *text, int max_width)
{
	unsigned char *split = text;

	if (max_width <= 0) return 0;

	while (*split && *split != '\n') {
		unsigned char *next_split = split + 1;

		while (is_unsplitable(next_split))
			next_split++;

		if (next_split - text > max_width) {
			/* Force a split if no position was found yet,
			 * meaning there's no splittable substring under
			 * requested width. */
			if (split == text) {
				split = &text[max_width];

				/* Give preference to split on a punctuation
				 * if any. Note that most of the time
				 * punctuation char is followed by a space so
				 * this rule will not match often.
				 * We match dash and quotes too. */
				while (--split != text) {
					if (!ispunct(*split)) continue;
					split++;
					break;
				}

				/* If no way to do a clean split, just return
				 * requested maximal width. */
				if (split == text)
					return max_width;
			}
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

		/* Skip first leading \n or space. */
		if (isspace(*text)) text++;
		if (!*text) break;

		width = split_line(text, max_width);

		/* split_line() may return 0. */
		if (width < 1) {
			width = 1; /* Infinite loop prevention. */
		}

		int_lower_bound(&widget_data->w, width);

		if (!realloc_lines(&lines, line, line + 1))
			break;

		lines[line++] = text;
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
		int x, int *y, int width, int *real_width,
		struct color_pair *color, enum format_align align)
{
	int line_width;
	int firstline = 1;

	for (; *text; text += line_width, (*y)++) {
		int shift;

		/* Skip first leading \n or space. */
		if (!firstline && isspace(*text))
			text++;
		else
			firstline = 0;
		if (!*text) break;

		line_width = split_line(text, width);

		/* split_line() may return 0. */
		if (line_width < 1) {
			line_width = 1; /* Infinite loop prevention. */
			continue;
		}

		if (real_width) int_lower_bound(real_width, line_width);
		if (!term || !line_width) continue;

		/* Calculate the number of chars to indent */
		if (align == AL_CENTER)
			shift = (width - line_width) / 2;
		else if (align == AL_RIGHT)
			shift = width - line_width;
		else
			shift = 0;

		assert(line_width <= width && shift < width);

		draw_text(term, x + shift, *y, text, line_width, 0, color);
	}
}

void
dlg_format_text(struct terminal *term, struct widget_data *widget_data,
		int x, int *y, int width, int *real_width, int max_height)
{
	unsigned char *text = widget_data->widget->text;
	unsigned char saved = 0;
	unsigned char *saved_pos = NULL;

	/* If we are drawing set up the dimensions before setting up the
	 * scrolling. */
	widget_data->x = x;
	widget_data->y = *y;
	widget_data->h = max_height - 3;
	if (widget_data->h <= 0) {
		widget_data->h = 0;
		return;
	}

	/* Can we scroll and do we even have to? */
	if (widget_data->widget->info.text.is_scrollable
	    && (widget_data->info.text.max_width != width
		|| widget_data->h < widget_data->info.text.lines))
	{
		unsigned char **lines;
		int current;
		int visible;

		/* Ensure that the current split is valid but don't
		 * split if we don't have to */
		if (widget_data->w != width
		    && !split_lines(widget_data, width))
			return;

		lines = (unsigned char **) widget_data->cdata;

		/* Make maximum number of lines available */
		visible = int_max(widget_data->info.text.lines - widget_data->h,
				  widget_data->h);

		int_bounds(&widget_data->info.text.current, 0, visible);
		current = widget_data->info.text.current;

		/* Set the current position */
		text = lines[current];

		/* Do we have to force a text end ? */
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
			*saved_pos = '\0';
		}

		/* Force dialog to be the width of the longest line */
		if (real_width) int_lower_bound(real_width, widget_data->w);

	} else {
		/* Always reset @current if we do not need to scroll */
		widget_data->info.text.current = 0;
	}

	dlg_format_text_do(term, text,
		x, y, width, real_width,
		term ? get_bfu_color(term, "dialog.text") : NULL,
		widget_data->widget->info.text.align);

	if (widget_data->widget->info.text.is_label) (*y)--;

	/* If we scrolled and something was trimmed restore it */
	if (saved && saved_pos) *saved_pos = saved;
}

static void
display_text(struct widget_data *widget_data, struct dialog_data *dlg_data, int sel)
{
	struct window *win = dlg_data->win;
	int x = dlg_data->x + dlg_data->width - DIALOG_LEFT_BORDER - 1;
	int y = widget_data->y;
	int height = widget_data->h;
	int scale, current, step;
	int lines = widget_data->info.text.lines;

	if (!text_is_scrollable(widget_data) || height <= 0) return;

	draw_area(win->term, x, y, 1, height, ' ', 0,
		    get_bfu_color(win->term, "dialog.scrollbar"));

	current = widget_data->info.text.current;
	scale = (height + 1) * 100 / lines;

	/* Scale the offset of @current */
	step = (current + 1) * scale / 100;
	int_bounds(&step, 0, widget_data->h - 1);

	/* Scale the number of visible lines */
	height = (height + 1) * scale / 100;
	int_bounds(&height, 1, int_max(widget_data->h - step, 1));

	/* Ensure we always step to the last position too */
	if (lines - widget_data->h == current) {
		step = widget_data->h - height;
	}
	y += step;

#ifdef CONFIG_MOUSE
	/* Save infos about selected scrollbar position and size.
	 * We'll use it when handling mouse. */
	widget_data->info.text.scroller_height = height;
	widget_data->info.text.scroller_y = y;
#endif

	draw_area(win->term, x, y, 1, height, ' ', 0,
		  get_bfu_color(win->term, "dialog.scrollbar-selected"));

	/* Hope this is at least a bit reasonable. Set cursor
	 * and window pointer to start of the first text line. */
	set_cursor(win->term, widget_data->x, widget_data->y, 0);
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

	assert(lines >= 0);
	assert(widget_data->h >= 0);

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
		case ACT_MAIN_UP:
		case ACT_MAIN_SCROLL_UP:
			current--;
			break;

		case ACT_MAIN_DOWN:
		case ACT_MAIN_SCROLL_DOWN:
			current++;
			break;

		case ACT_MAIN_PAGE_UP:
			current -= widget_data->h;
			break;

		case ACT_MAIN_PAGE_DOWN:
			current += widget_data->h;
			break;

		case ACT_MAIN_HOME:
			current = 0;
			break;

		case ACT_MAIN_END:
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
#ifdef CONFIG_MOUSE
	int x = dlg_data->x + dlg_data->width - DIALOG_LEFT_BORDER - 1;
	int y = widget_data->y;
	int height = widget_data->h;
	int current = widget_data->info.text.current;
	int scroller_y = widget_data->info.text.scroller_y;
	int scroller_height = widget_data->info.text.scroller_height;
	int scroller_middle = scroller_y + scroller_height/2
			      - widget_data->info.text.scroller_last_dir;

	if (ev->x != x || ev->y < y || ev->y >= y + height)
		return EVENT_NOT_PROCESSED;

	switch (get_mouse_button(ev)) {
	case B_LEFT:
		/* Left click scrolls up or down by one step. */
		if (ev->y <= scroller_middle)
			current--;
		else
			current++;
		break;

	case B_RIGHT:
		/* Right click scrolls up or down by more than one step.
		 * Faster. */
		if (ev->y <= scroller_middle)
			current -= 5;
		else
			current += 5;
		break;

	case B_WHEEL_UP:
		/* Mouse wheel up scrolls up. */
		current--;
		break;

	case B_WHEEL_DOWN:
		/* Mouse wheel up scrolls down. */
		current++;
		break;

	default:
		return EVENT_NOT_PROCESSED;
	}

	/* Save last direction used. */
	if (widget_data->info.text.current < current)
		widget_data->info.text.scroller_last_dir = 1;
	else
		widget_data->info.text.scroller_last_dir = -1;

	format_and_display_text(widget_data, dlg_data, current);

#endif /* CONFIG_MOUSE */

	return EVENT_PROCESSED;
}


struct widget_ops text_ops = {
	display_text,
	NULL,
	mouse_text,
	kbd_text,
	NULL,
};
