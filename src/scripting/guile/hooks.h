/* $Id: hooks.h,v 1.1 2003/07/24 15:33:33 pasky Exp $ */

#ifndef EL__SCRIPTING_GUILE_HOOKS_H
#define EL__SCRIPTING_GUILE_HOOKS_H

#ifdef HAVE_GUILE

#include "sched/session.h"

unsigned char *script_hook_goto_url(struct session *, unsigned char *);
unsigned char *script_hook_follow_url(struct session *, unsigned char *);
unsigned char *script_hook_pre_format_html(struct session *, unsigned char *, unsigned char *, int *);
unsigned char *script_hook_get_proxy(unsigned char *);
void script_hook_quit(void);

#endif

#endif
