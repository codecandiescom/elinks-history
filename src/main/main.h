/* $Id: main.h,v 1.2 2002/03/17 13:54:11 pasky Exp $ */

#ifndef EL__MAIN_H
#define EL__MAIN_H

#include <lowlevel/terminal.h>

extern unsigned char *path_to_exe;

void unhandle_terminal_signals(struct terminal *term);
/* int attach_terminal(int, int, int, void *, int); */
void shrink_memory(int);

#endif
