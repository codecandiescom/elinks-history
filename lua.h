/* $Id: lua.h,v 1.1 2002/03/16 22:03:09 pasky Exp $ */

#ifndef EL__LUA_H
#define EL__LUA_H

#ifdef HAVE_LUA

#include "session.h"

extern lua_State *lua_state;

void init_lua();
void alert_lua_error(unsigned char *);
void alert_lua_error2(unsigned char *, unsigned char *);
int prepare_lua(struct session *);
void finish_lua();
void lua_console(struct session *, unsigned char *);
void handle_standard_lua_returns(unsigned char *);

#endif

#endif
