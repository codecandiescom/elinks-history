/* $Id: main.h,v 1.14 2003/07/25 13:07:13 pasky Exp $ */

#ifndef EL__MAIN_H
#define EL__MAIN_H

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
