/* $Id: core.h,v 1.12 2003/10/26 14:02:35 jonas Exp $ */

#ifndef EL__SCRIPTING_LUA_CORE_H
#define EL__SCRIPTING_LUA_CORE_H

#ifdef HAVE_LUA

#include <lua.h> /* This is standart include. */

#include "modules/module.h"
#include "sched/event.h"
#include "sched/session.h"

extern lua_State *lua_state;

int prepare_lua(struct session *);
void finish_lua(void);

void alert_lua_error(unsigned char *);
void alert_lua_error2(unsigned char *, unsigned char *);

enum evhook_status dialog_lua_console(va_list ap, void *data);
enum evhook_status free_lua_console_history(va_list ap, void *data);

extern struct module lua_scripting_module;

#endif

#endif
