/* $Id: screen.h,v 1.5 2003/07/28 08:25:21 jonas Exp $ */

#ifndef EL__TERMINAL_SCREEN_H
#define EL__TERMINAL_SCREEN_H

#define get_screen_char_data(x)	((unsigned char) ((x) & 0xff))
#define get_screen_char_attr(x)	((unsigned char) ((x) >> 8))
#define encode_screen_char(x)	((unsigned) (x).data + ((x).attr << 8))

/* One position in the terminal screen's image. */
struct screen_char {
	/* Contains either character value or frame data. */
	unsigned char data;

	/* Attributes includes color and frame info. */
	unsigned char attr;
};

/* The terminal's screen manages */
struct terminal_screen {
	/* This is the screen's image, character by character. */
	struct screen_char *image;

	/* The previous screen's image, used for optimizing actual drawing. */
	struct screen_char *last_image;
};

void alloc_screen(struct terminal *term, int x, int y);
void done_screen(struct terminal_screen *screen);
void redraw_screen(struct terminal *);
void erase_screen(struct terminal *);
void beep_terminal(struct terminal *);

#endif
