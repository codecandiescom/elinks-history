/* $Id: draw.h,v 1.18 2003/08/01 22:19:33 jonas Exp $ */

#ifndef EL__TERMINAL_DRAW_H
#define EL__TERMINAL_DRAW_H

#include "terminal/terminal.h"

#define SCREEN_ATTR_FRAME	0x80

/* One position in the terminal screen's image. */
struct screen_char {
	/* Contains either character value or frame data. */
	unsigned char data;

	/* Attributes includes color and frame info. */
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

void set_char(struct terminal *, int, int, unsigned char, unsigned char);
void set_border_char(struct terminal *term, int x, int y, enum border_char border, unsigned char color);
void set_xchar(struct terminal *, int x, int y, enum frame_cross_direction);
struct screen_char *get_char(struct terminal *, int, int);
void set_color(struct terminal *, int, int, unsigned char);
void set_only_char(struct terminal *, int, int, unsigned char);
void set_line(struct terminal *, int, int, int, struct screen_char *);
void fill_area(struct terminal *, int, int, int, int, unsigned char, unsigned char);
void draw_frame(struct terminal *, int, int, int, int, unsigned char, int);
void print_text(struct terminal *, int, int, int, unsigned char *, unsigned char);

#define fill_border_area(t, x, y, xw, yw, d, c) do { \
		fill_area(t, x, y, xw, yw, d, c | SCREEN_ATTR_FRAME); \
	} while (0)

/* Updates the terminals cursor position. When @blockable is set the
 * block_cursor terminal option decides whether the cursor should be put at the
 * bottom right corner of the screen. */
void set_cursor(struct terminal *term, int x, int y, int blockable);

void clear_terminal(struct terminal *);

#endif
