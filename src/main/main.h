/* $Id: main.h,v 1.16 2004/04/14 19:43:02 jonas Exp $ */

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
extern int ac;
extern unsigned char **av;

void shrink_memory(int);

#endif
