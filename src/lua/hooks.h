/* $Id: hooks.h,v 1.1 2002/05/07 13:19:44 pasky Exp $ */

#ifndef EL__LUA_HOOKS_H
#define EL__LUA_HOOKS_H

#ifdef HAVE_LUA

#include <document/session.h>
#include <lowlevel/sched.h>

void script_hook_goto_url(struct session *, unsigned char *);
unsigned char *script_hook_follow_url(struct session *, unsigned char *);
unsigned char *script_hook_pre_format_html(struct session *, unsigned char *, unsigned char *, int *);
unsigned char *script_hook_get_proxy(unsigned char *);
void script_hook_quit(void);

#endif

#endif
