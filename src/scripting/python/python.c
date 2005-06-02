/* Python module */
/* $Id: python.c,v 1.1 2005/06/02 18:01:34 witekfl Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "modules/module.h"
#include "scripting/python/core.h"
#include "scripting/python/hooks.h"


struct module python_scripting_module = struct_module(
	/* name: */		"Python",
	/* options: */		NULL,
	/* hooks: */		python_scripting_hooks,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		init_python,
	/* done: */		cleanup_python
);
