/* General scripting system functionality */
/* $Id: scripting.c,v 1.4 2003/09/23 20:30:43 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_SCRIPTING

#include "elinks.h"

#include "sched/event.h"
#include "scripting/scripting.h"


/* Backends dynamic area: */

#include "scripting/guile/core.h"
#include "scripting/lua/core.h"

static struct scripting_backend *scripting_backends[] = {
#ifdef HAVE_LUA
	&lua_scripting_backend,
#endif
#ifdef HAVE_GUILE
	&guile_scripting_backend,
#endif
	NULL,
};

static inline void
register_scripting_hooks(struct scripting_hook *hooks)
{
	int i;

	for (i = 0; hooks[i].name; i++) {
		int id = register_event(hooks[i].name);

		if (id == EVENT_NONE) continue;

		register_event_hook(id, hooks[i].callback, 0);
	}
}

static inline void
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
	int i;

	for (i = 0; scripting_backends[i]; i++) {
		struct scripting_backend *backend = scripting_backends[i];

		if (backend->init)
			backend->init();

		if (backend->hooks)
			register_scripting_hooks(backend->hooks);
	}
}

void
done_scripting(void)
{
	int i;

	for (i = 0; scripting_backends[i]; i++) {
		struct scripting_backend *backend = scripting_backends[i];

		if (backend->hooks)
			unregister_scripting_hooks(backend->hooks);

		if (backend->done)
			backend->done();
	}
}

#endif
