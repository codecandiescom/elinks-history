/* $Id: main.h,v 1.3 2002/03/16 22:03:09 pasky Exp $ */

#ifndef EL__MAIN_H
#define EL__MAIN_H

#include "terminal.h"

extern unsigned char *path_to_exe;

void unhandle_terminal_signals(struct terminal *term);
/* int attach_terminal(int, int, int, void *, int); */
void shrink_memory(int);

#endif
