/* $Id: lua.h,v 1.2 2002/03/17 13:54:14 pasky Exp $ */

#ifndef EL__LUA_H
#define EL__LUA_H

#ifdef HAVE_LUA

#include <document/session.h>

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
