/* General scripting system functionality */
/* $Id: scripting.c,v 1.3 2003/09/23 18:44:54 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_SCRIPTING

#include "elinks.h"

#include "sched/event.h"
#include "scripting/guile/core.h"
#include "scripting/lua/core.h"
#include "scripting/scripting.h"


void
register_scripting_hooks(struct scripting_hook *hooks)
{
	int i;

	for (i = 0; hooks[i].name; i++) {
		int id = register_event(hooks[i].name);

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

void
init_scripting(void)
{
#ifdef HAVE_LUA
	init_lua();
#endif
#ifdef HAVE_GUILE
	init_guile();
#endif
}

void
done_scripting(void)
{
#ifdef HAVE_LUA
	cleanup_lua();
#endif
#ifdef HAVE_GUILE
	done_guile();
#endif
}

#endif
