/* $Id: core.h,v 1.5 2003/06/15 22:44:14 pasky Exp $ */

#ifndef EL__SCRIPTING_LUA_CORE_H
#define EL__SCRIPTING_LUA_CORE_H

#ifdef HAVE_LUA

#include <lua.h> /* This is standart include. */
#include "sched/session.h"

extern lua_State *lua_state;

void init_lua(void);
int prepare_lua(struct session *);
void finish_lua(void);
void cleanup_lua(void);

void alert_lua_error(unsigned char *);
void alert_lua_error2(unsigned char *, unsigned char *);

void dialog_lua_console(struct session *);
void free_lua_console_history(void);

void run_lua_func(struct session *, int);

#endif

#endif
