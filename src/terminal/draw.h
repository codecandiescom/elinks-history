/* $Id: draw.h,v 1.19 2003/08/03 03:44:24 jonas Exp $ */

#ifndef EL__TERMINAL_DRAW_H
#define EL__TERMINAL_DRAW_H

#include "terminal/terminal.h"

enum screen_char_attr {
	SCREEN_ATTR_ITALIC	= 0x10,
	SCREEN_ATTR_UNDERLINE	= 0x20,
	SCREEN_ATTR_BOLD	= 0x40,
	SCREEN_ATTR_FRAME	= 0x80,
};

/* One position in the terminal screen's image. */
struct screen_char {
	/* Contains either character value or frame data. */
	unsigned char data;

	/* The encoded fore- and background color. */
	unsigned char color;

	/* Attributes are screen_char_attr bits. */
	unsigned char attr;
};

/* Linux frame symbols table (it's magically converted to other terminals when
 * needed). */
/* In the screen image, they have attribute SCREEN_ATTR_FRAME; you should drop them
 * to the image using draw_frame_char(). */
/* TODO: When we'll support internal Unicode, this should be changed to some
 * Unicode sequences. --pasky */

enum border_char {
	/* single-lined */
	BORDER_SULCORNER = 218,
	BORDER_SURCORNER = 191,
	BORDER_SDLCORNER = 192,
	BORDER_SDRCORNER = 217,
	BORDER_SLTEE	 = 180, /* => the tee points to the left => -| */
	BORDER_SRTEE	 = 195,
	BORDER_SVLINE	 = 179,
	BORDER_SHLINE	 = 196,
	BORDER_SCROSS	 = 197, /* + */

	/* double-lined */ /* TODO: The TEE-chars! */
	BORDER_DULCORNER = 201,
	BORDER_DURCORNER = 187,
	BORDER_DDLCORNER = 200,
	BORDER_DDRCORNER = 188,
	BORDER_DVLINE	 = 186,
	BORDER_DHLINE	 = 205,
};

/* 0 -> 1 <- 2 v 3 ^ */
enum frame_cross_direction {
	FRAME_X_RIGHT = 0,
	FRAME_X_LEFT,
	FRAME_X_DOWN,
	FRAME_X_UP
};

void draw_char(struct terminal *, int, int, unsigned char, unsigned char, unsigned char);
void set_border_char(struct terminal *term, int x, int y, enum border_char border, unsigned char color);
void set_xchar(struct terminal *, int x, int y, enum frame_cross_direction);
struct screen_char *get_char(struct terminal *, int, int);
void set_color(struct terminal *, int, int, unsigned char);
void set_only_char(struct terminal *, int, int, unsigned char);
void set_line(struct terminal *, int, int, int, struct screen_char *);
void draw_area(struct terminal *, int, int, int, int, unsigned char, unsigned char, unsigned char color);
void draw_frame(struct terminal *, int, int, int, int, unsigned char, int);
void draw_text(struct terminal *, int, int, int, unsigned char *, unsigned char, unsigned char color);

#define set_char(t, x, y, data, attr) draw_char(t, x, y, data, attr, attr)
#define fill_area(t, x, y, xw, yw, d, c) draw_area(t, x, y, xw, yw, d, c, c)
#define print_text(t, x, y, l, txt, c) draw_text(t, x, y, l, txt, c, c)

#define draw_border_area(t, x, y, xw, yw, d, c) do { \
		draw_area(t, x, y, xw, yw, d, c, SCREEN_ATTR_FRAME); \
	} while (0)

/* Updates the terminals cursor position. When @blockable is set the
 * block_cursor terminal option decides whether the cursor should be put at the
 * bottom right corner of the screen. */
void set_cursor(struct terminal *term, int x, int y, int blockable);

void clear_terminal(struct terminal *);

#endif
