/* Public terminal drawing API. Frontend for the screen image in memory. */
/* $Id: draw.c,v 1.87 2004/05/13 09:06:32 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "config/options.h"
#include "terminal/color.h"
#include "terminal/draw.h"
#include "terminal/screen.h"
#include "terminal/terminal.h"
#include "util/color.h"
#include "util/rect.h"

/* Makes sure that @x and @y are within the dimensions of the terminal. */
#define check_range(term, x, y) \
	do { \
		int_bounds(&(x), 0, (term)->width - 1); \
		int_bounds(&(y), 0, (term)->height - 1); \
	} while (0)

#ifdef CONFIG_256_COLORS
#define clear_screen_char_color(schar) do { memset((schar)->color, 0, 2); } while (0)
#else
#define clear_screen_char_color(schar) do { (schar)->color[0] = 0; } while (0)
#endif

/* TODO: Clearify this piece of magic code. --jonas */
void
draw_border_cross(struct terminal *term, int x, int y,
		  enum border_cross_direction dir, struct color_pair *color)
{
	static unsigned char border_trans[2][4] = {
		{ BORDER_SVLINE, BORDER_SRTEE, BORDER_SLTEE, BORDER_SCROSS },
		{ BORDER_SHLINE, /* ? */ 0xc2, /* ? */ 0xc1, BORDER_SCROSS },
	};
	struct screen_char *screen_char;
	unsigned int d;

	assert(term && term->screen && color);
	if_assert_failed return;
	check_range(term, x, y);

	screen_char = get_char(term, x, y);
	if (!(screen_char->attr & SCREEN_ATTR_FRAME)) return;

	d = dir>>1;
	if (screen_char->data == border_trans[d][0]) {
		screen_char->data = border_trans[d][1 + (dir & 1)];

	} else if (screen_char->data == border_trans[d][2 - (dir & 1)]) {
		screen_char->data = border_trans[d][3];
	}

	set_term_color(screen_char, color, 0,
		       get_opt_int_tree(term->spec, "colors"));
}

void
draw_border_char(struct terminal *term, int x, int y,
		 enum border_char border, struct color_pair *color)
{
	int position;

	assert(term && term->screen && term->screen->image && color);
	if_assert_failed return;
	check_range(term, x, y);

	position = x + term->width * y;
	term->screen->image[position].data = (unsigned char) border;
	term->screen->image[position].attr = SCREEN_ATTR_FRAME;
	set_term_color(&term->screen->image[position], color, 0,
		       get_opt_int_tree(term->spec, "colors"));
	set_screen_dirty(term->screen, y, y);
}


struct screen_char *
get_char(struct terminal *term, int x, int y)
{
	assert(term && term->screen && term->screen->image);
	if_assert_failed return 0;
	check_range(term, x, y);

	return &term->screen->image[x + term->width * y];
}

void
draw_char_color(struct terminal *term, int x, int y, struct color_pair *color)
{
	int position;

	assert(term && term->screen && term->screen->image);
	if_assert_failed return;
	check_range(term, x, y);

	position = x + term->width * y;
	set_term_color(&term->screen->image[position], color, 0,
		       get_opt_int_tree(term->spec, "colors"));
	set_screen_dirty(term->screen, y, y);
}

void
draw_char_data(struct terminal *term, int x, int y, unsigned char data)
{
	assert(term && term->screen && term->screen->image);
	if_assert_failed return;
	check_range(term, x, y);

	term->screen->image[x + term->width * y].data = data;
	set_screen_dirty(term->screen, y, y);
}

/* Updates a line in the terms screen. */
/* When doing frame drawing @x can be different than 0. */
void
draw_line(struct terminal *term, int x, int y, int l, struct screen_char *line)
{
	register int position, size;

	assert(term && term->screen && term->screen->image && line);
	if_assert_failed return;
	check_range(term, x, y);

	size = int_min(l, term->width - x);
	if (size == 0) return;

	position = x + term->width * y;
	copy_screen_chars(&term->screen->image[position], line, size);
	set_screen_dirty(term->screen, y, y);
}

void
draw_border(struct terminal *term, int x, int y, int xw, int yw,
	   struct color_pair *color, int width)
{
	static enum border_char p1[] = {
		BORDER_SULCORNER,
		BORDER_SURCORNER,
		BORDER_SDLCORNER,
		BORDER_SDRCORNER,
		BORDER_SVLINE,
		BORDER_SHLINE,
	};
	static enum border_char p2[] = {
		BORDER_DULCORNER,
		BORDER_DURCORNER,
		BORDER_DDLCORNER,
		BORDER_DDRCORNER,
		BORDER_DVLINE,
		BORDER_DHLINE,
	};
	enum border_char *p = (width > 1) ? p2 : p1;
	int xt = x + xw - 1;
	int yt = y + yw - 1;
	int y1 = y + 1;
	int x1 = x + 1;
	int ywt = yw - 2;
	int xwt = xw - 2;

	draw_area(term, x, y1, 1, ywt, p[4], SCREEN_ATTR_FRAME, color);
	draw_area(term, xt, y1, 1, ywt, p[4], SCREEN_ATTR_FRAME, color);
	draw_area(term, x1, y, xwt, 1, p[5], SCREEN_ATTR_FRAME, color);
	draw_area(term, x1, yt, xwt, 1, p[5], SCREEN_ATTR_FRAME, color);

	if (xw > 1 && yw > 1) {
		draw_border_char(term, x, y, p[0], color);
		draw_border_char(term, xt, y, p[1], color);
		draw_border_char(term, x, yt, p[2], color);
		draw_border_char(term, xt, yt, p[3], color);
	}

	set_screen_dirty(term->screen, y, y + yw);
}

void
draw_char(struct terminal *term, int x, int y,
	  unsigned char data, enum screen_char_attr attr,
	  struct color_pair *color)
{
	int position;

	assert(term && term->screen);
	if_assert_failed return;
	check_range(term, x, y);

	position = x + term->width * y;
	term->screen->image[position].data = data;
	term->screen->image[position].attr = attr;
	set_term_color(&term->screen->image[position], color, 0,
		       get_opt_int_tree(term->spec, "colors"));

	set_screen_dirty(term->screen, y, y);
}

void
draw_area(struct terminal *term, int x, int y, int xw, int yw,
	  unsigned char data, enum screen_char_attr attr,
	  struct color_pair *color)
{
	struct screen_char *line, *pos, *end;
	int width, height;

	assert(term && term->screen && term->screen->image);
	if_assert_failed return;
	check_range(term, x, y);

	height = int_min(yw, term->height - y);
	width = int_min(xw, term->width - x);

	if (height <= 0 || width <= 0) return;

	line = &term->screen->image[x + term->width * y];

	/* Compose off the ending screen position in the areas first line. */
	end = &line[width - 1];
	end->attr = attr;
	end->data = data;
	if (color) {
		set_term_color(end, color, 0,
			       get_opt_int_tree(term->spec, "colors"));
	} else {
		clear_screen_char_color(end);
	}

	/* Draw the first area line. */
	for (pos = line; pos < end; pos++) {
		copy_screen_chars(pos, end, 1);
	}

	/* Now make @end point to the last line */
	/* For the rest of the area use the first area line. */
	for (pos = line + term->width, height -= 1;
	     height;
	     height--, pos += term->width) {
		copy_screen_chars(pos, line, width);
	}

	set_screen_dirty(term->screen, y, y + yw);
}

void
draw_box(struct terminal *term, struct rect *box,
	 unsigned char data, enum screen_char_attr attr,
	 struct color_pair *color)
{
	/* draw_area() may disappear later. --Zas */
	draw_area(term, box->x,  box->y, box->width, box->height,
		  data, attr, color);
}

void
draw_shadow_box(struct terminal *term, struct rect *box,
		struct color_pair *color, int width, int height)
{
	struct rect dbox;

	/* (horizontal) */
	set_rect(&dbox,
		 box->x + width,
		 box->y + box->height,
		 box->width - width,
		 height);

	draw_box(term, &dbox, ' ', 0, color);

	/* (vertical) */
	set_rect(&dbox,
		 box->x + box->width,
		 box->y + height,
		 width,
		 box->height);

	draw_box(term, &dbox, ' ', 0, color);
}

void
draw_text(struct terminal *term, int x, int y,
	  unsigned char *text, int length,
	  enum screen_char_attr attr, struct color_pair *color)
{
	struct screen_char *pos, *end;

	assert(term && term->screen && term->screen->image && text && length >= 0);
	if_assert_failed return;

	if (length <= 0) return;

	check_range(term, x, y);

	pos = &term->screen->image[x + term->width * y];
	end = &pos[int_min(length, term->width - x) - 1];

	if (color) {
		/* Use the last char as template. */
		memset(end, 0, sizeof(struct screen_char));
		end->attr = attr;
		set_term_color(end, color, 0,
			       get_opt_int_tree(term->spec, "colors"));

		for (; pos < end && *text; text++, pos++) {
			end->data = *text;
			copy_screen_chars(pos, end, 1);
		}

		end->data = *text;

	} else {
		for (; pos <= end && *text; text++, pos++) {
			pos->data = *text;
		}
	}

	set_screen_dirty(term->screen, y, y);
}

void
set_cursor(struct terminal *term, int x, int y, int blockable)
{
	assert(term && term->screen);
	if_assert_failed return;
	check_range(term, x, y);

	if (blockable && get_opt_bool_tree(term->spec, "block_cursor")) {
		x = term->width - 1;
		y = term->height - 1;
	}

	if (term->screen->cx != x || term->screen->cy != y) {
		set_screen_dirty(term->screen, int_min(term->screen->cy, y),
					       int_max(term->screen->cy, y));
		term->screen->cx = x;
		term->screen->cy = y;
	}
}

void
clear_terminal(struct terminal *term)
{
	draw_area(term, 0, 0, term->width, term->height, ' ', 0, NULL);
	set_cursor(term, 0, 0, 1);
}
