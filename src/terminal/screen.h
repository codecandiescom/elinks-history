/* $Id: screen.h,v 1.4 2003/07/27 21:22:54 jonas Exp $ */

#ifndef EL__TERMINAL_SCREEN_H
#define EL__TERMINAL_SCREEN_H

#include "terminal/terminal.h"

#define get_screen_char_data(x)	((unsigned char) ((x) & 0xff))
#define get_screen_char_attr(x)	((unsigned char) ((x) >> 8))
#define encode_screen_char(x)	((unsigned) (x).data + ((x).attr << 8))

void alloc_screen(struct terminal *term, int x, int y);
void redraw_screen(struct terminal *);
void erase_screen(struct terminal *);
void beep_terminal(struct terminal *);

#endif
