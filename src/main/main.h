/* $Id: main.h,v 1.5 2003/04/14 15:05:47 zas Exp $ */

#ifndef EL__MAIN_H
#define EL__MAIN_H

#include "lowlevel/terminal.h"

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
void init();

#endif
