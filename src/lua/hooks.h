/* $Id: hooks.h,v 1.3 2003/01/01 20:30:34 pasky Exp $ */

#ifndef EL__LUA_HOOKS_H
#define EL__LUA_HOOKS_H

#ifdef HAVE_LUA

#include "document/session.h"
#include "sched/sched.h"

void script_hook_goto_url(struct session *, unsigned char *);
unsigned char *script_hook_follow_url(struct session *, unsigned char *);
unsigned char *script_hook_pre_format_html(struct session *, unsigned char *, unsigned char *, int *);
unsigned char *script_hook_get_proxy(unsigned char *);
void script_hook_quit(void);

#endif

#endif
