/* $Id: hooks.h,v 1.8 2003/09/22 21:54:47 jonas Exp $ */

#ifndef EL__SCRIPTING_LUA_HOOKS_H
#define EL__SCRIPTING_LUA_HOOKS_H

#ifdef HAVE_LUA

void register_lua_hooks(void);
void unregister_lua_hooks(void);

#endif

#endif
