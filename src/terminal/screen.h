/* $Id: screen.h,v 1.3 2003/07/25 13:23:51 pasky Exp $ */

#ifndef EL__TERMINAL_SCREEN_H
#define EL__TERMINAL_SCREEN_H

#include "terminal/terminal.h"

void alloc_screen(struct terminal *term, int x, int y);
void redraw_screen(struct terminal *);
void erase_screen(struct terminal *);
void beep_terminal(struct terminal *);

#endif
