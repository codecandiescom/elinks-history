/* $Id: core.h,v 1.10 2003/10/02 12:54:36 kuser Exp $ */

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

enum evhook_status dialog_lua_console(va_list ap, void *data);
enum evhook_status free_lua_console_history(va_list ap, void *data);

extern struct scripting_backend lua_scripting_backend;

#endif

#endif
