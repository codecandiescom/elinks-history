/* Public terminal drawing API. Frontend for the screen image in memory. */
/* $Id: draw.c,v 1.28 2003/07/30 21:38:43 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "config/options.h"
#include "terminal/draw.h"
#include "terminal/screen.h"
#include "terminal/terminal.h"


void
set_char(struct terminal *term, int x, int y, unsigned c)
{
	struct terminal_screen *screen = term->screen;
	int position = x + term->x * y;

	assert(x >= 0 && x < term->x && y >= 0 && y < term->y);
	if_assert_failed { return; }

	screen->image[position].data = get_screen_char_data(c);
	screen->image[position].attr = get_screen_char_attr(c);
	screen->dirty = 1;
}

unsigned char frame_trans[2][4] = {{0xb3, 0xc3, 0xb4, 0xc5}, {0xc4, 0xc2, 0xc1, 0xc5}};

void
set_xchar(struct terminal *term, int x, int y, enum frame_cross_direction dir)
{
	unsigned int d;
	struct screen_char *screen_char;

	assert(term);
	if_assert_failed return;

	assert(x >= 0 && x < term->x && y >= 0 && y < term->y);
	if_assert_failed return;

	screen_char = get_char(term, x, y);
	if (!(screen_char->attr & SCREEN_ATTR_FRAME)) return;

	d = dir>>1;
	if (screen_char->data == frame_trans[d][0]) {
		screen_char->data = frame_trans[d][1 + (dir & 1)];

	} else if (screen_char->data == frame_trans[d][2 - (dir & 1)]) {
		screen_char->data = frame_trans[d][3];
	}
}


struct screen_char *
get_char(struct terminal *term, int x, int y)
{
	assert(x >= 0 && x < term->x && y >= 0 && y < term->y);
	if_assert_failed { return 0; }

	return &term->screen->image[x + term->x * y];
}

void
set_color(struct terminal *term, int x, int y, unsigned c)
{
	struct terminal_screen *screen = term->screen;
	int position = x + term->x * y;

	assert(x >= 0 && x < term->x && y >= 0 && y < term->y);
	if_assert_failed { return; }

	c = get_screen_char_attr(c) & ~SCREEN_ATTR_FRAME;
	screen->image[position].attr = c | (screen->image[position].attr & SCREEN_ATTR_FRAME);
	screen->dirty = 1;
}

void
set_only_char(struct terminal *term, int x, int y, unsigned c)
{
	struct terminal_screen *screen = term->screen;
	int position = x + term->x * y;

	assert(x >= 0 && x < term->x && y >= 0 && y < term->y);
	if_assert_failed { return; }

	screen->image[position].data = get_screen_char_data(c);
	screen->dirty = 1;
}

/* Updates a line in the terms screen. */
/* When doing frame drawing @x can be different than 0. */
void
set_line(struct terminal *term, int x, int y, int l, struct screen_char *line)
{
	struct terminal_screen *screen = term->screen;
	int end = (l <= term->x - x) ? l : term->x - x;
	register int offset = x + term->x * y;
	register int i;

	assert(line);
	if_assert_failed { return; }

	assert(l >= 0 && l <= term->x && x >= 0 && x < term->x && y >= 0 && y < term->y);
	if_assert_failed { return; }

	if (end == 0) return;

	for (i = 0; i < end; i++) {
		int position = i + offset;

		screen->image[position].data = line[i].data;
		screen->image[position].attr = line[i].attr;
	}
	screen->dirty = 1;
}

void
fill_area(struct terminal *term, int x, int y, int xw, int yw, unsigned c)
{
	struct terminal_screen *screen = term->screen;
	int starty = (y >= 0) ? 0 : -y;
	int startx = (x >= 0) ? 0 : -x;
	int endy = (yw < term->y - y) ? yw : term->y - y;
	int endx = (xw < term->x - x) ? xw : term->x - x;
	int offset_base = x + term->x * y;
	register int j;

	assert(x >= 0 && x < term->x && y >= 0 && y < term->y);
	if_assert_failed { return; }

	for (j = starty; j < endy; j++) {
		register int offset = offset_base + term->x * j;
		register int position = startx + offset;

		/* No, we can't use memset() here :(. It's int, not char. */
		/* TODO: Make screen two arrays actually. Enables various
		 * optimalizations, consumes nearly same memory. --pasky */
		for (; position < endx + offset; position++) {
			screen->image[position].data = get_screen_char_data(c);
			screen->image[position].attr = get_screen_char_attr(c);
		}
	}
	screen->dirty = 1;
}

void
draw_frame(struct terminal *t, int x, int y, int xw, int yw,
	   unsigned c, int w)
{
	static enum frame_char p1[] = {
		FRAMES_ULCORNER,
		FRAMES_URCORNER,
		FRAMES_DLCORNER,
		FRAMES_DRCORNER,
		FRAMES_VLINE,
		FRAMES_HLINE,
	};
	static enum frame_char p2[] = {
		FRAMED_ULCORNER,
		FRAMED_URCORNER,
		FRAMED_DLCORNER,
		FRAMED_DRCORNER,
		FRAMED_VLINE,
		FRAMED_HLINE,
	};
	enum frame_char *p = (w > 1) ? p2 : p1;
	int xt = x + xw - 1;
	int yt = y + yw - 1;
	int y1 = y + 1;
	int x1 = x + 1;
	int ywt = yw - 2;
	int xwt = xw - 2;
	int cp4 = c + p[4];
	int cp5 = c + p[5];

	set_char(t, x, y, c + p[0]);
	set_char(t, xt, y, c + p[1]);
	set_char(t, x, yt, c + p[2]);
	set_char(t, xt, yt, c + p[3]);

	fill_area(t, x, y1, 1, ywt, cp4);
	fill_area(t, xt, y1, 1, ywt, cp4);
	fill_area(t, x1, y, xwt, 1, cp5);
	fill_area(t, x1, yt, xwt, 1, cp5);
}

void
print_text(struct terminal *term, int x, int y, int l,
	   unsigned char *text, unsigned c)
{
	struct terminal_screen *screen = term->screen;
	int end = (l <= term->x - x) ? l : term->x - x;
	int position = x + term->x * y;

	assert(text && l >= 0);
	if_assert_failed { return; }
	assert(x >= 0 && x < term->x && y >= 0 && y < term->y);
	if_assert_failed { return; }

	for (end += position; position < end && *text; text++, position++) {
		screen->image[position].data = *text;
		screen->image[position].attr = get_screen_char_attr(c);
	}
	screen->dirty = 1;
}


void
set_cursor(struct terminal *term, int x, int y, int blockable)
{
	struct terminal_screen *screen = term->screen;

	assert(x >= 0 && x < term->x && y >= 0 && y < term->y);
	if_assert_failed { return; }

	if (blockable && get_opt_bool_tree(term->spec, "block_cursor")) {
		x = term->x - 1;
		y = term->y - 1;
	}

	if (screen->cx != x || screen->cy != y) {
		screen->cx = x;
		screen->cy = y;
		screen->dirty = 1;
	}
}

void
clear_terminal(struct terminal *term)
{
	fill_area(term, 0, 0, term->x, term->y, ' ');
	set_cursor(term, 0, 0, 0);
}
