/* $Id: scripting.h,v 1.3 2003/09/23 14:06:10 pasky Exp $ */

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

#endif

#endif
