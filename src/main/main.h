/* $Id: main.h,v 1.19 2004/04/15 02:24:00 jonas Exp $ */

#ifndef EL__MAIN_H
#define EL__MAIN_H

enum retval {
	RET_OK,
	RET_ERROR,
	RET_SIGNAL,
	RET_SYNTAX,
	RET_FATAL,
	RET_PING,
	RET_REMOTE,
};

extern enum retval retval;
extern int terminate;
extern unsigned char *path_to_exe;

void shrink_memory(int);

#endif
