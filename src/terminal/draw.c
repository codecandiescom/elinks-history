/* Public terminal drawing API. Frontend for the screen image in memory. */
/* $Id: draw.c,v 1.13 2003/07/27 21:22:54 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "config/options.h"
#include "terminal/draw.h"
#include "terminal/screen.h"
#include "terminal/terminal.h"


void
set_char(struct terminal *t, int x, int y, unsigned c)
{
	int position = x + t->x * y;

	assert(x >= 0 && x < t->x && y >= 0 && y < t->y);
	if_assert_failed { return; }

	t->screen[position].data = get_screen_char_data(c);
	t->screen[position].attr = get_screen_char_attr(c);
	t->dirty = 1;
}

unsigned
get_char(struct terminal *t, int x, int y)
{
	assert(x >= 0 && x < t->x && y >= 0 && y < t->y);
	if_assert_failed { return 0; }

	return encode_screen_char(t->screen[x + t->x * y]);
}

void
set_color(struct terminal *t, int x, int y, unsigned c)
{
	int position;

	assert(x >= 0 && x < t->x && y >= 0 && y < t->y);
	if_assert_failed { return; }

	position = x + t->x * y;
	t->screen[position].attr = (t->screen[position].attr & 0x80) | (get_screen_char_attr(c) & ~0x80);
	t->dirty = 1;
}

void
set_only_char(struct terminal *t, int x, int y, unsigned c)
{
	int position;

	assert(x >= 0 && x < t->x && y >= 0 && y < t->y);
	if_assert_failed { return; }

	position = x + t->x * y;
	t->screen[position].data = get_screen_char_data(c);
	t->dirty = 1;
}

/* Updates a line in the terms screen. */
/* When doing frame drawing @x can be different than 0. */
void
set_line(struct terminal *t, int x, int y, int l, chr *line)
{
	register int i;
	int end = (x + l <= t->x) ? l : t->x - x;
	register int offset = x + t->x * y;

	assert(l >= 0 && l <= t->x && x >= 0 && x < t->x && y >= 0 && y < t->y);
	if_assert_failed { return; }

	if (end == 0) return;

	assert(line);
	if_assert_failed { return; }

	for (i = 0; i < end; i++) {
		int position = i + offset;

		t->screen[position].data = get_screen_char_data(line[i]);
		t->screen[position].attr = get_screen_char_attr(line[i]);
	}
	t->dirty = 1;
}

#if 0
void
set_line_color(struct terminal *t, int x, int y, int l, unsigned c)
{
	register int i = (x >= 0) ? 0 : -x;
	int end = (x + l <= t->x) ? l : t->x - x;
	int offset = x + t->x * y;

	if (i >= end) return;

	for (; i < end; i++) {
		register int p = i + offset;

		t->screen[p] = (t->screen[p] & 0x80ff) | (c & ~0x80ff);
	}
	t->dirty = 1;
}
#endif

void
fill_area(struct terminal *t, int x, int y, int xw, int yw, unsigned c)
{
	int starty = (y >= 0) ? 0 : -y;
	int startx = (x >= 0) ? 0 : -x;
	int endy = (yw < t->y - y) ? yw : t->y - y;
	int endx = (xw < t->x - x) ? xw : t->x - x;
	int offset_base = x + t->x * y;
	register int j;

	assert(x >= 0 && x < t->x && y >= 0 && y < t->y);
	if_assert_failed { return; }

	for (j = starty; j < endy; j++) {
		register int offset = offset_base + t->x * j;
		register int i;

		/* No, we can't use memset() here :(. It's int, not char. */
		/* TODO: Make screen two arrays actually. Enables various
		 * optimalizations, consumes nearly same memory. --pasky */
		for (i = startx; i < endx; i++) {
			int position = i + offset;

			t->screen[position].data = get_screen_char_data(c);
			t->screen[position].attr = get_screen_char_attr(c);
		}
	}
	t->dirty = 1;
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
print_text(struct terminal *t, int x, int y, int l,
		unsigned char *text, unsigned c)
{
	assert(text && l >= 0);
	if_assert_failed { return; }
	assert(x >= 0 && x < t->x && y >= 0 && y < t->y);
	if_assert_failed { return; }

	l = (x + l <= t->x) ? l : t->x - x;
	for (; l-- && *text; text++, x++)
		set_char(t, x, y, *text + c);
}


void
set_cursor(struct terminal *term, int x, int y, int blockable)
{
	assert(x >= 0 && x < term->x && y >= 0 && y < term->y);
	if_assert_failed { return; }

	if (blockable && get_opt_bool_tree(term->spec, "block_cursor")) {
		x = term->x - 1;
		y = term->y - 1;
	}

	if (term->cx != x || term->cy != y) {
		term->cx = x;
		term->cy = y;
		term->dirty = 1;
	}
}

void
clear_terminal(struct terminal *term)
{
	fill_area(term, 0, 0, term->x, term->y, ' ');
	set_cursor(term, 0, 0, 0);
}
