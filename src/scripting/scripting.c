/* General scripting system functionality */
/* $Id: scripting.c,v 1.7 2003/10/26 13:46:14 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_SCRIPTING

#include "elinks.h"

#include "modules/module.h"
#include "sched/event.h"
#include "scripting/scripting.h"


/* Backends dynamic area: */

#include "scripting/guile/core.h"
#include "scripting/lua/core.h"

static struct module *scripting_modules[] = {
#ifdef HAVE_LUA
	&lua_scripting_module,
#endif
#ifdef HAVE_GUILE
	&guile_scripting_module,
#endif
	NULL,
};

struct module scripting_module = module_struct(
	/* name: */		"scripting",
	/* options: */		NULL,
	/* events: */		NULL,
	/* submodules: */	scripting_modules,
	/* data: */		NULL,
	/* init: */		NULL,
	/* done: */		NULL
);

#endif
