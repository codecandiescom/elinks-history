/* $Id: hooks.h,v 1.4 2003/10/26 13:13:42 jonas Exp $ */

#ifndef EL__SCRIPTING_GUILE_HOOKS_H
#define EL__SCRIPTING_GUILE_HOOKS_H

#ifdef HAVE_GUILE

#include "sched/event.h"

extern struct event_hook_info guile_scripting_hooks[];

#endif

#endif
