/* $Id: hooks.h,v 1.1 2004/03/02 17:06:15 witekfl Exp $ */

#ifndef EL__SCRIPTING_PERL_HOOKS_H
#define EL__SCRIPTING_PERL_HOOKS_H

#ifdef HAVE_PERL

#include "sched/event.h"

extern struct event_hook_info perl_scripting_hooks[];

#endif

#endif
