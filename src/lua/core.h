/* $Id: core.h,v 1.1 2002/05/07 13:19:44 pasky Exp $ */

#ifndef EL__LUA_CORE_H
#define EL__LUA_CORE_H

#ifdef HAVE_LUA

#include <lua.h> /* This is standart include. */
#include <document/session.h>

extern lua_State *lua_state;

void init_lua(void);
int prepare_lua(struct session *);
void finish_lua(void);

void alert_lua_error(unsigned char *);
void alert_lua_error2(unsigned char *, unsigned char *);

void dialog_lua_console(struct session *);
void free_lua_console_history(void);

void run_lua_func(struct session *, int);

#endif

#endif
