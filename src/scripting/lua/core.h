/* $Id: core.h,v 1.8 2003/10/02 11:09:12 kuser Exp $ */

#ifndef EL__SCRIPTING_LUA_CORE_H
#define EL__SCRIPTING_LUA_CORE_H

#ifdef HAVE_LUA

#include <lua.h> /* This is standart include. */

#include "sched/session.h"
#include "scripting/scripting.h"

extern lua_State *lua_state;

int prepare_lua(struct session *);
void finish_lua(void);

void alert_lua_error(unsigned char *);
void alert_lua_error2(unsigned char *, unsigned char *);

void dialog_lua_console(struct session *);
void free_lua_console_history(void);

extern struct scripting_backend lua_scripting_backend;

#endif

#endif
