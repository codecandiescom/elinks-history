/* $Id: rect.h,v 1.3 2004/05/10 17:15:22 zas Exp $ */

#ifndef EL__UTIL_RECT_H
#define EL__UTIL_RECT_H

struct rect {
	int x;
	int y;
	int width;
	int height;
};

static inline int
is_in_rect(struct rect *rect, int x, int y)
{
	return (x >= rect->x && y >= rect->y
		&& x < rect->x + rect->width
		&& y < rect->y + rect->height);
}

static inline int
row_is_in_rect(struct rect *rect, int y)
{
	return (y >= rect->y && y < rect->y + rect->height);
}

static inline int
col_is_in_rect(struct rect *rect, int x)
{
	return (x >= rect->x && x < rect->x + rect->width);
}


static inline void
set_rect(struct rect *rect, int x, int y, int width, int height)
{
	rect->x = x;
	rect->y = y;
	rect->width = width;
	rect->height = height;
}

static inline void
copy_rect(struct rect *dst, struct rect *src)
{
	memcpy(dst, src, sizeof(struct rect));
}

#define dbg_show_rect(rect) DBG("x=%i y=%i width=%i height=%i", (rect)->x, (rect)->y, (rect)->width, (rect)->height)
#define dbg_show_xy(x_, y_) DBG("x=%i y=%i", x_, y_)


#endif
