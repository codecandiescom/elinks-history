/* General scripting system functionality */
/* $Id: scripting.c,v 1.6 2003/10/26 13:13:42 jonas Exp $ */

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

void
init_scripting(void)
{
	int i;

	for (i = 0; scripting_backends[i]; i++) {
		struct scripting_backend *backend = scripting_backends[i];

		if (backend->init)
			backend->init();

		if (backend->hooks)
			register_event_hooks(backend->hooks);
	}
}

void
done_scripting(void)
{
	int i;

	for (i = 0; scripting_backends[i]; i++) {
		struct scripting_backend *backend = scripting_backends[i];

		if (backend->hooks)
			unregister_event_hooks(backend->hooks);

		if (backend->done)
			backend->done();
	}
}

#endif
