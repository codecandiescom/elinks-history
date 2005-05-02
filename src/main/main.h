/* $Id: main.h,v 1.22 2005/05/02 20:29:06 jonas Exp $ */

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
	RET_COMMAND,
};

struct program {
	int terminate;
	enum retval retval;
	unsigned char *path;
};

extern struct program program;

void shrink_memory(int);

#endif
