/* $Id: main.h,v 1.13 2003/06/14 20:02:34 pasky Exp $ */

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
