#ifndef EL__MAIN_H
#define EL__MAIN_H

//#include "terminal.h"

extern unsigned char *path_to_exe;

void unhandle_terminal_signals(struct terminal *term);
/* int attach_terminal(int, int, int, void *, int); */
void shrink_memory(int);

#endif
