/* $Id: hooks.h,v 1.5 2003/06/15 22:44:14 pasky Exp $ */

#ifndef EL__SCRIPTING_LUA_HOOKS_H
#define EL__SCRIPTING_LUA_HOOKS_H

#ifdef HAVE_LUA

#include "sched/sched.h"
#include "sched/session.h"

void script_hook_goto_url(struct session *, unsigned char *);
unsigned char *script_hook_follow_url(struct session *, unsigned char *);
unsigned char *script_hook_pre_format_html(struct session *, unsigned char *, unsigned char *, int *);
unsigned char *script_hook_get_proxy(unsigned char *);
void script_hook_quit(void);

#endif

#endif
