/* $Id: screen.h,v 1.17 2003/09/08 22:19:39 jonas Exp $ */

#ifndef EL__TERMINAL_SCREEN_H
#define EL__TERMINAL_SCREEN_H

#include "terminal/draw.h"

/* The terminal's screen manages */
struct terminal_screen {
	/* This is the screen's image, character by character. */
	struct screen_char *image;

	/* The previous screen's image, used for optimizing actual drawing. */
	struct screen_char *last_image;

	/* The current and the previous cursor positions. */
	int cx, cy;
	int lcx, lcy;

	/* We are sure that @screen and the physical screen are out of sync. */
	unsigned int dirty:1;
};

/* Mark the screen ready for redrawing. */
#define set_screen_dirty(s) do { (s)->dirty = 1; } while (0)

/* Initializes a screen. Returns NULL upon allocation failure. */
struct terminal_screen *init_screen(void);

/* Cleans up after the screen. */
void done_screen(struct terminal_screen *screen);

/* Update the size of the previous and the current screen image to hold @x time
 * @y chars. */
void resize_screen(struct terminal *term, int x, int y);

/* Updates the terminal screen. */
void redraw_screen(struct terminal *term);

/* Erases the entire screen and moves the cursor to the upper left corner. */
void erase_screen(struct terminal *term);

/* Meeep! */
void beep_terminal(struct terminal *term);

/* Release private screen drawing utilities. */
void done_screen_drivers(void);

#endif
