/* $Id: draw.h,v 1.5 2003/07/27 17:23:53 jonas Exp $ */

#ifndef EL__TERMINAL_DRAW_H
#define EL__TERMINAL_DRAW_H

#include "terminal/terminal.h"

typedef unsigned short chr;

#define ATTR_FRAME      0x8000

/* Linux frame symbols table (it's magically converted to other terminals when
 * needed). */
/* In the screen image, they have attribute ATTR_FRAME; you should drop them
 * to the image using draw_frame_char(). */
/* TODO: When we'll support internal Unicode, this should be changed to some
 * Unicode sequences. --pasky */

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

void set_char(struct terminal *, int, int, unsigned);
unsigned get_char(struct terminal *, int, int);
void set_color(struct terminal *, int, int, unsigned);
void set_only_char(struct terminal *, int, int, unsigned);
void set_line(struct terminal *, int, int, int, chr *);
void fill_area(struct terminal *, int, int, int, int, unsigned);
void draw_frame(struct terminal *, int, int, int, int, unsigned, int);
void print_text(struct terminal *, int, int, int, unsigned char *, unsigned);

/* Updates the terminals cursor position. When @blockable is set the
 * block_cursor terminal option decides whether the cursor should be put at the
 * bottom right corner of the screen. */
void set_cursor(struct terminal *term, int x, int y, int blockable);

void clear_terminal(struct terminal *);

#endif
