/* $Id: scripting.h,v 1.4 2003/09/23 18:44:54 jonas Exp $ */

#ifndef EL__SCRIPTING_SCRIPTING_H
#define EL__SCRIPTING_SCRIPTING_H

#ifdef HAVE_SCRIPTING

#include "sched/event.h"

struct scripting_hook {
	unsigned char *name;
	event_hook callback;
};

/* Plugs hooks into the event system. */
/* XXX: Last entry of @hooks must have a NULL @name. */
void register_scripting_hooks(struct scripting_hook *hooks);
void unregister_scripting_hooks(struct scripting_hook *hooks);

void init_scripting(void);
void done_scripting(void);

#endif

#endif
