/* General scripting system functionality */
/* $Id: scripting.c,v 1.17 2005/04/01 17:42:56 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "intl/gettext/libintl.h"
#include "modules/module.h"
#include "sched/event.h"
#include "scripting/scripting.h"


/* Backends dynamic area: */

#ifdef CONFIG_GUILE
#include "scripting/guile/guile.h"
#endif
#ifdef CONFIG_LUA
#include "scripting/lua/lua.h"
#endif
#ifdef CONFIG_PERL
#include "scripting/perl/perl.h"
#endif
#ifdef CONFIG_RUBY
#include "scripting/ruby/ruby.h"
#endif


static struct module *scripting_modules[] = {
#ifdef CONFIG_LUA
	&lua_scripting_module,
#endif
#ifdef CONFIG_GUILE
	&guile_scripting_module,
#endif
#ifdef CONFIG_PERL
	&perl_scripting_module,
#endif
#ifdef CONFIG_RUBY
	&ruby_scripting_module,
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
