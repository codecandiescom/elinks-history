/* $Id: main.h,v 1.3 2002/03/17 22:32:23 pasky Exp $ */

#ifndef EL__MAIN_H
#define EL__MAIN_H

#include <lowlevel/terminal.h>

enum retval {
	RET_OK,
	RET_ERROR,
	RET_SIGNAL,
	RET_SYNTAX,
	RET_FATAL,
};

extern enum retval retval;

extern unsigned char *path_to_exe;

void unhandle_terminal_signals(struct terminal *term);
/* int attach_terminal(int, int, int, void *, int); */
void shrink_memory(int);

#endif
