/* $Id: hooks.h,v 1.10 2003/10/26 13:13:42 jonas Exp $ */

#ifndef EL__SCRIPTING_LUA_HOOKS_H
#define EL__SCRIPTING_LUA_HOOKS_H

#ifdef HAVE_LUA

#include "sched/event.h"

extern struct event_hook_info lua_scripting_hooks[];

#endif

#endif
