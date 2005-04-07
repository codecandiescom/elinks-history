/* $Id: main.h,v 1.20 2005/04/07 10:45:42 zas Exp $ */

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

struct program {
	int terminate;
	enum retval retval;
	unsigned char *path_to_exe;
};

extern struct program program;

void shrink_memory(int);

#endif
