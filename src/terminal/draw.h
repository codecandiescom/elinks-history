/* $Id: draw.h,v 1.11 2003/07/31 01:09:06 jonas Exp $ */

#ifndef EL__TERMINAL_DRAW_H
#define EL__TERMINAL_DRAW_H

#include "terminal/screen.h"
#include "terminal/terminal.h"

#define ATTR_FRAME 		0x8000
#define SCREEN_ATTR_FRAME	0x80

#define get_screen_char_data(x)	((unsigned char) ((x) & 0xff))
#define get_screen_char_attr(x)	((unsigned char) ((x) >> 8))
#define encode_screen_char(x)	((unsigned) (x).data + ((x).attr << 8))

/* Linux frame symbols table (it's magically converted to other terminals when
 * needed). */
/* In the screen image, they have attribute ATTR_FRAME; you should drop them
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

enum frame_char {
	/* single-lined */
	FRAMES_ULCORNER = 218 | ATTR_FRAME,
	FRAMES_URCORNER = 191 | ATTR_FRAME,
	FRAMES_DLCORNER = 192 | ATTR_FRAME,
	FRAMES_DRCORNER = 217 | ATTR_FRAME,
	FRAMES_LTEE = 180 | ATTR_FRAME, /* => the tee points to the left => -| */
	FRAMES_RTEE = 195 | ATTR_FRAME,
	FRAMES_VLINE = 179 | ATTR_FRAME,
	FRAMES_HLINE = 196 | ATTR_FRAME,
	FRAMES_CROSS = 197 | ATTR_FRAME, /* + */

	/* double-lined */ /* TODO: The TEE-chars! */
	FRAMED_ULCORNER = 201 | ATTR_FRAME,
	FRAMED_URCORNER = 187 | ATTR_FRAME,
	FRAMED_DLCORNER = 200 | ATTR_FRAME,
	FRAMED_DRCORNER = 188 | ATTR_FRAME,
	FRAMED_VLINE = 186 | ATTR_FRAME,
	FRAMED_HLINE = 205 | ATTR_FRAME,
};

/* 0 -> 1 <- 2 v 3 ^ */
enum frame_cross_direction {
	FRAME_X_RIGHT = 0,
	FRAME_X_LEFT,
	FRAME_X_DOWN,
	FRAME_X_UP
};

void set_char(struct terminal *, int, int, unsigned char, unsigned int);
void set_border_char(struct terminal *term, int x, int y, enum border_char border, unsigned char color);
void set_xchar(struct terminal *, int x, int y, enum frame_cross_direction);
struct screen_char *get_char(struct terminal *, int, int);
void set_color(struct terminal *, int, int, unsigned);
void set_only_char(struct terminal *, int, int, unsigned);
void set_line(struct terminal *, int, int, int, struct screen_char *);
void fill_area(struct terminal *, int, int, int, int, unsigned);
void draw_frame(struct terminal *, int, int, int, int, unsigned, int);
void print_text(struct terminal *, int, int, int, unsigned char *, unsigned);

/* Updates the terminals cursor position. When @blockable is set the
 * block_cursor terminal option decides whether the cursor should be put at the
 * bottom right corner of the screen. */
void set_cursor(struct terminal *term, int x, int y, int blockable);

void clear_terminal(struct terminal *);

#endif
