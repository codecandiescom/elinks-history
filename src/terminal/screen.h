/* $Id: screen.h,v 1.2 2003/05/04 19:38:05 pasky Exp $ */

#ifndef EL__TERMINAL_SCREEN_H
#define EL__TERMINAL_SCREEN_H

#include "terminal/terminal.h"

void redraw_screen(struct terminal *);
void erase_screen(struct terminal *);
void beep_terminal(struct terminal *);

#endif
