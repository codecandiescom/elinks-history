/* Public terminal drawing API. Frontend for the screen image in memory. */
/* $Id: draw.c,v 1.2 2003/05/04 19:36:42 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "config/options.h"
#include "terminal/draw.h"
#include "terminal/terminal.h"


void
set_char(struct terminal *t, int x, int y, unsigned c)
{
	t->dirty = 1;
	if (x >= 0 && x < t->x && y >= 0 && y < t->y)
		t->screen[x + t->x * y] = c;
}

unsigned
get_char(struct terminal *t, int x, int y)
{
	if (x >= t->x) x = t->x - 1;
	if (x < 0) x = 0;
	if (y >= t->y) y = t->y - 1;
	if (y < 0) y = 0;

	return t->screen[x + t->x * y];
}

void
set_color(struct terminal *t, int x, int y, unsigned c)
{
	t->dirty = 1;
	if (x >= 0 && x < t->x && y >= 0 && y < t->y) {
		int p = x + t->x * y;

		t->screen[p] = (t->screen[p] & 0x80ff) | (c & ~0x80ff);
	}
}

void
set_only_char(struct terminal *t, int x, int y, unsigned c)
{
	t->dirty = 1;
	if (x >= 0 && x < t->x && y >= 0 && y < t->y) {
		int p = x + t->x * y;

		t->screen[p] = (t->screen[p] & ~0x80ff) | (c & 0x80ff);
	}
}

void
set_line(struct terminal *t, int x, int y, int l, chr *line)
{
	int i = (x >= 0) ? 0 : -x;
	int end = (x + l <= t->x) ? l : t->x - x;

	t->dirty = 1;

	for (; i < end; i++)
		t->screen[x + i + t->x * y] = line[i];
}

void
set_line_color(struct terminal *t, int x, int y, int l, unsigned c)
{
	int i = (x >= 0) ? 0 : -x;
	int end = (x + l <= t->x) ? l : t->x - x;

	t->dirty = 1;

	for (; i < end; i++) {
		int p = x + i + t->x * y;

		t->screen[p] = (t->screen[p] & 0x80ff) | (c & ~0x80ff);
	}
}

void
fill_area(struct terminal *t, int x, int y, int xw, int yw, unsigned c)
{
	int j = (y >= 0) ? 0 : -y;

	t->dirty = 1;
	for (; j < yw && y + j < t->y; j++) {
		int i;

		/* No, we can't use memset() here :(. It's int, not char. */
		/* TODO: Make screen two arrays actually. Enables various
		 * optimalizations, consumes nearly same memory. --pasky */
		for (i = (x >= 0) ? 0 : -x; i < xw && x + i < t->x; i++)
			t->screen[x + i + t->x * (y + j)] = c;
	}
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
	enum frame_char *p = w > 1 ? p2 : p1;

	set_char(t, x, y, c+p[0]);
	set_char(t, x+xw-1, y, c+p[1]);
	set_char(t, x, y+yw-1, c+p[2]);
	set_char(t, x+xw-1, y+yw-1, c+p[3]);
	fill_area(t, x, y+1, 1, yw-2, c+p[4]);
	fill_area(t, x+xw-1, y+1, 1, yw-2, c+p[4]);
	fill_area(t, x+1, y, xw-2, 1, c+p[5]);
	fill_area(t, x+1, y+yw-1, xw-2, 1, c+p[5]);
}

void
print_text(struct terminal *t, int x, int y, int l,
		unsigned char *text, unsigned c)
{
	for (; l-- && *text; text++, x++) set_char(t, x, y, *text + c);
}


/* (altx,alty) is alternative location, when block_cursor terminal option is
 * set. It is usually bottom right corner of the screen. */
void
set_cursor(struct terminal *term, int x, int y, int altx, int alty)
{
	term->dirty = 1;
	if (get_opt_bool_tree(term->spec, "block_cursor")) {
		x = altx;
		y = alty;
	}
	if (x >= term->x) x = term->x - 1;
	if (y >= term->y) y = term->y - 1;
	if (x < 0) x = 0;
	if (y < 0) y = 0;
	term->cx = x;
	term->cy = y;
}

void
clear_terminal(struct terminal *term)
{
	fill_area(term, 0, 0, term->x, term->y, ' ');
	set_cursor(term, 0, 0, 0, 0);
}
