/* Public terminal drawing API. Frontend for the screen image in memory. */
/* $Id: draw.c,v 1.44 2003/08/01 10:39:45 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "config/options.h"
#include "terminal/draw.h"
#include "terminal/screen.h"
#include "terminal/terminal.h"


#define check_range(term, x, y) \
	do { \
		int_upper_bound(&(x), (term)->x - 1); \
		int_lower_bound(&(x), 0); \
		int_upper_bound(&(y), (term)->y - 1); \
		int_lower_bound(&(y), 0); \
	} while (0)

void
set_char(struct terminal *term, int x, int y,
	 unsigned char data, unsigned char attr)
{
	int position;

	assert(term && term->screen);
	if_assert_failed return;
	check_range(term, x, y);

	position = x + term->x * y;
	term->screen->image[position].data = data;
	term->screen->image[position].attr = attr;
	term->screen->dirty = 1;
}

void
set_xchar(struct terminal *term, int x, int y, enum frame_cross_direction dir)
{
	static unsigned char frame_trans[2][4] = {{0xb3, 0xc3, 0xb4, 0xc5},
						  {0xc4, 0xc2, 0xc1, 0xc5}};
	struct screen_char *screen_char;
	unsigned int d;

	assert(term && term->screen);
	if_assert_failed return;
	check_range(term, x, y);

	screen_char = get_char(term, x, y);
	if (!(screen_char->attr & SCREEN_ATTR_FRAME)) return;

	d = dir>>1;
	if (screen_char->data == frame_trans[d][0]) {
		screen_char->data = frame_trans[d][1 + (dir & 1)];

	} else if (screen_char->data == frame_trans[d][2 - (dir & 1)]) {
		screen_char->data = frame_trans[d][3];
	}
}

void
set_border_char(struct terminal *term, int x, int y,
	        enum border_char border, unsigned char color)
{
	int position;

	assert(term && term->screen && term->screen->image);
	if_assert_failed return;
	check_range(term, x, y);

	/* This should probably go.  */
	if (!color)
		color = get_char(term, x, y)->attr;

	position = x + term->x * y;
	term->screen->image[position].data = (unsigned char) border;
	term->screen->image[position].attr = (color | SCREEN_ATTR_FRAME);
	term->screen->dirty = 1;
}


struct screen_char *
get_char(struct terminal *term, int x, int y)
{
	assert(term && term->screen && term->screen->image);
	if_assert_failed return 0;
	check_range(term, x, y);

	return &term->screen->image[x + term->x * y];
}

void
set_color(struct terminal *term, int x, int y, unsigned char color)
{
	unsigned char attr = color & ~SCREEN_ATTR_FRAME;
	int position;

	assert(term && term->screen && term->screen->image);
	if_assert_failed return;
	check_range(term, x, y);

	position = x + term->x * y;
	attr |= (term->screen->image[position].attr & SCREEN_ATTR_FRAME);
	term->screen->image[position].attr = attr;
	term->screen->dirty = 1;
}

void
set_only_char(struct terminal *term, int x, int y, unsigned char data)
{
	assert(term && term->screen && term->screen->image);
	if_assert_failed return;
	check_range(term, x, y);

	term->screen->image[x + term->x * y].data = data;
	term->screen->dirty = 1;
}

/* Updates a line in the terms screen. */
/* When doing frame drawing @x can be different than 0. */
void
set_line(struct terminal *term, int x, int y, int l, struct screen_char *line)
{
	int position, end;
	register int i, j;

	assert(term && term->screen && term->screen->image && line);
	if_assert_failed return;
	check_range(term, x, y);

	end = int_min(l, term->x - x);
	if (end == 0) return;

	position = x + term->x * y;
	for (i = position, j = 0; i < end + position; i++, j++) {
		term->screen->image[i].data = line[j].data;
		term->screen->image[i].attr = line[j].attr;
	}
	term->screen->dirty = 1;
}

void
fill_area(struct terminal *term, int x, int y, int xw, int yw,
	  unsigned char data, unsigned char attr)
{
	int position;
	int endx, endy;
	register int j;

	assert(term && term->screen && term->screen->image);
	if_assert_failed return;
	check_range(term, x, y);

	endx = int_min(xw, term->x - x);
	endy = int_min(yw, term->y - y);

	position = x + term->x * y;

	for (j = 0; j < endy; j++) {
		register int offset = position + term->x * j;
		register int i;

		/* No, we can't use memset() here :(. It's int, not char. */
		/* TODO: Make screen two arrays actually. Enables various
		 * optimalizations, consumes nearly same memory. --pasky */
		for (i = offset; i < endx + offset; i++) {
			term->screen->image[i].data = data;
			term->screen->image[i].attr = attr;
		}
	}
	term->screen->dirty = 1;
}

void
draw_frame(struct terminal *term, int x, int y, int xw, int yw,
	   unsigned char color, int w)
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
	enum frame_char *p = (w > 1) ? p2 : p1;
	int xt = x + xw - 1;
	int yt = y + yw - 1;
	int y1 = y + 1;
	int x1 = x + 1;
	int ywt = yw - 2;
	int xwt = xw - 2;

	set_border_char(term, x, y, p[0], color);
	set_border_char(term, xt, y, p[1], color);
	set_border_char(term, x, yt, p[2], color);
	set_border_char(term, xt, yt, p[3], color);

	fill_border_area(term, x, y1, 1, ywt, p[4], color);
	fill_border_area(term, xt, y1, 1, ywt, p[4], color);
	fill_border_area(term, x1, y, xwt, 1, p[5], color);
	fill_border_area(term, x1, yt, xwt, 1, p[5], color);
}

void
print_text(struct terminal *term, int x, int y, int l,
	   unsigned char *text, unsigned char color)
{
	int position, end;

	assert(term && term->screen && term->screen->image && text && l >= 0);
	if_assert_failed return;
	check_range(term, x, y);

	end = int_min(l, term->x - x);
	position = x + term->x * y;

	for (end += position; position < end && *text; text++, position++) {
		term->screen->image[position].data = *text;
		term->screen->image[position].attr = color;
	}
	term->screen->dirty = 1;
}


void
set_cursor(struct terminal *term, int x, int y, int blockable)
{
	assert(term && term->screen);
	if_assert_failed return;
	check_range(term, x, y);

	if (blockable && get_opt_bool_tree(term->spec, "block_cursor")) {
		x = term->x - 1;
		y = term->y - 1;
	}

	if (term->screen->cx != x || term->screen->cy != y) {
		term->screen->cx = x;
		term->screen->cy = y;
		term->screen->dirty = 1;
	}
}

void
clear_terminal(struct terminal *term)
{
	fill_area(term, 0, 0, term->x, term->y, ' ', 0);
	set_cursor(term, 0, 0, 0);
}
