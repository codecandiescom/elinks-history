/* $Id: scripting.h,v 1.7 2003/10/01 11:31:50 jonas Exp $ */

#ifndef EL__SCRIPTING_SCRIPTING_H
#define EL__SCRIPTING_SCRIPTING_H

#ifdef HAVE_SCRIPTING

#include "sched/event.h"

#define NULL_SCRIPTING_HOOK { NULL, NULL, NULL }

struct scripting_hook {
	unsigned char *name;
	event_hook callback;
	void *data;
};

struct scripting_backend {
	void (*init)(void);
	void (*done)(void);

	/* Hooks that should be plugged into the event system. */
	/* XXX: Last entry of @hooks must be NULL_SCRIPTING_HOOK. */
	struct scripting_hook *hooks;
};

void init_scripting(void);
void done_scripting(void);

#endif

#endif
