/* $Id: scripting.h,v 1.8 2003/10/26 13:13:42 jonas Exp $ */

#ifndef EL__SCRIPTING_SCRIPTING_H
#define EL__SCRIPTING_SCRIPTING_H

#ifdef HAVE_SCRIPTING

#include "sched/event.h"

struct scripting_backend {
	void (*init)(void);
	void (*done)(void);

	/* Hooks that should be plugged into the event system. */
	/* XXX: Last entry of @hooks must be NULL_EVENT_HOOK_INFO. */
	struct event_hook_info *hooks;
};

void init_scripting(void);
void done_scripting(void);

#endif

#endif
