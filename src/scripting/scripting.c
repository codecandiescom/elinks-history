/* General scripting system functionality */
/* $Id: scripting.c,v 1.9 2003/10/27 01:12:15 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_SCRIPTING

#include "elinks.h"

#include "intl/gettext/libintl.h"
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

struct module scripting_module = struct_module(
	/* name: */		N_("Scripting"),
	/* options: */		NULL,
	/* events: */		NULL,
	/* submodules: */	scripting_modules,
	/* data: */		NULL,
	/* init: */		NULL,
	/* done: */		NULL
);

#endif
