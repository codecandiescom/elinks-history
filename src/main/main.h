/* $Id: main.h,v 1.9 2003/05/23 21:22:20 zas Exp $ */

#ifndef EL__MAIN_H
#define EL__MAIN_H

#include "terminal/terminal.h"

enum retval {
	RET_OK,
	RET_ERROR,
	RET_SIGNAL,
	RET_SYNTAX,
	RET_FATAL,
};

extern enum retval retval;
extern int terminate;
extern unsigned char *path_to_exe;

void shrink_memory(int);
void init(void);

#endif
