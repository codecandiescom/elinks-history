/* $Id: main.h,v 1.15 2004/03/02 17:06:15 witekfl Exp $ */

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
void init(void);

#endif
