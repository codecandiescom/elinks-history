/* General scripting system functionality */
/* $Id: scripting.c,v 1.1 2003/09/23 00:45:18 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_SCRIPTING

#include "elinks.h"

#include "sched/event.h"
#include "scripting/scripting.h"


void
register_scripting_hooks(struct scripting_hook *hooks)
{
	int i;

	for (i = 0; hooks[i].name; i++) {
		int id = get_event_id(hooks[i].name);

		if (id == EVENT_NONE) continue;

		register_event_hook(id, hooks[i].callback, 0);
	}
}

void
unregister_scripting_hooks(struct scripting_hook *hooks)
{
	int i;

	for (i = 0; hooks[i].name; i++) {
		int id = get_event_id(hooks[i].name);

		if (id == EVENT_NONE) continue;

		unregister_event_hook(id, hooks[i].callback);
	}
}

#endif
