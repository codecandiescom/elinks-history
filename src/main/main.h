/* $Id: main.h,v 1.10 2003/06/13 17:04:39 zas Exp $ */

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
#ifdef USE_FASTFIND
extern void *ff_info_tags;
#endif

void shrink_memory(int);
void init(void);

#endif
