/* $Id: rect.h,v 1.1 2004/05/09 21:47:48 zas Exp $ */

#ifndef EL__UTIL_RECT_H
#define EL__UTIL_RECT_H

struct rect {
	int x;
	int y;
	int width;
	int height;
};

#define is_in_rect(rect, x_, y_) ((x_) >= (rect).x && (y_) >= (rect).y	\
		                  && (x_) < (rect).x + (rect).width	\
				  && (y_) < (rect).y + (rect).height)

#define set_rect(rect, x_, y_, width_, height_) do { \
	(rect).x = (x_); \
	(rect).y = (y_); \
	(rect).width = (width_); \
	(rect).height = (height_); \
} while (0)

#define dbg_show_rect(rect) DBG("x=%i y=%i width=%i height=%i", (rect).x, (rect).y, (rect).width, (rect).height)
#define dbg_show_xy(x_, y_) DBG("x=%i y=%i", x_, y_)


#endif
