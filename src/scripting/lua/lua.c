/* Lua module */
/* $Id: lua.c,v 1.1 2005/04/01 09:41:37 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "modules/module.h"
#include "scripting/lua/core.h"
#include "scripting/lua/hooks.h"


struct module lua_scripting_module = struct_module(
	/* name: */		"Lua",
	/* options: */		NULL,
	/* hooks: */		lua_scripting_hooks,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		init_lua,
	/* done: */		cleanup_lua
);
