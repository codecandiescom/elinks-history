/* Perl module */
/* $Id: perl.c,v 1.1 2005/04/01 09:47:28 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "modules/module.h"
#include "scripting/perl/core.h"
#include "scripting/perl/hooks.h"


struct module perl_scripting_module = struct_module(
	/* name: */		"Perl",
	/* options: */		NULL,
	/* hooks: */		perl_scripting_hooks,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		init_perl,
	/* done: */		cleanup_perl
);
