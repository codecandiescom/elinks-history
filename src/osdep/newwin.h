/* $Id: newwin.h,v 1.5 2004/04/17 01:10:39 jonas Exp $ */

#ifndef EL__DIALOG_SYSTEM_H
#define EL__DIALOG_SYSTEM_H

#include "terminal/terminal.h"

struct open_in_new {
	enum term_env_type env;
	void (*fn)(struct terminal *term, unsigned char *, unsigned char *);
	unsigned char *text;
};

struct open_in_new *get_open_in_new(struct terminal * term);
int can_open_in_new(struct terminal *);

void open_new_window(struct terminal *term, unsigned char *exe_name,
		     enum term_env_type environment, unsigned char *param);

#endif
