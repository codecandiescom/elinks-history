/* $Id: scripting.h,v 1.5 2003/09/23 20:30:43 jonas Exp $ */

#ifndef EL__SCRIPTING_SCRIPTING_H
#define EL__SCRIPTING_SCRIPTING_H

#ifdef HAVE_SCRIPTING

#include "sched/event.h"

struct scripting_hook {
	unsigned char *name;
	event_hook callback;
};

struct scripting_backend {
	void (*init)(void);
	void (*done)(void);

	/* Hooks that should be plugged into the event system. */
	/* XXX: Last entry of @hooks must have a NULL @name. */
	struct scripting_hook *hooks;
};

void init_scripting(void);
void done_scripting(void);

#endif

#endif
