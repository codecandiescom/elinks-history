/* $Id: newwin.h,v 1.7 2004/04/17 02:09:15 jonas Exp $ */

#ifndef EL__DIALOG_SYSTEM_H
#define EL__DIALOG_SYSTEM_H

#include "terminal/terminal.h"

struct open_in_new {
	enum term_env_type env;
	unsigned char *command;
	unsigned char *text;
};

#define foreach_open_in_new(i, term_env) \
	for ((i) = 0; open_in_new[(i)].env; (i)++) \
		if (((term_env) & open_in_new[(i)].env))

extern const struct open_in_new open_in_new[];

int can_open_in_new(struct terminal *);

void open_new_window(struct terminal *term, unsigned char *exe_name,
		     enum term_env_type environment, unsigned char *param);

#endif
