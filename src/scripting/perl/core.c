/* Perl scripting engine */
/* $Id: core.c,v 1.4 2004/04/21 08:59:29 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_PERL

#include "elinks.h"

#include "main.h"
#include "lowlevel/home.h"
#include "scripting/perl/core.h"
#include "scripting/perl/hooks.h"
#include "util/file.h"

#define PERL_HOOKS_FILENAME	"hooks.pl"

PerlInterpreter *my_perl;

#ifdef PERL_SYS_INIT3
extern char **environ;
#endif

static char *
get_global_hook_file(void)
{
	static char buf[256];

	snprintf(buf, 256, "%s/%s", CONFDIR, PERL_HOOKS_FILENAME);
	if (file_exists(buf)) return buf;
	return NULL;
}

static char *
get_local_hook_file(void)
{
	static char buf[256];

	if (!elinks_home) return NULL;
	snprintf(buf, 256, "%s/%s", elinks_home, PERL_HOOKS_FILENAME);
	if (file_exists(buf)) return buf;
	return NULL;
}

static void
precleanup_perl(struct module *module)
{
	if (my_perl) {
		perl_destruct(my_perl);
		perl_free(my_perl);
		my_perl = NULL;
	}
}


static void
cleanup_perl(struct module *module)
{
	precleanup_perl(module);
#ifdef PERL_SYS_TERM
	PERL_SYS_TERM();
#endif
}

static void
init_perl(struct module *module)
{
/* FIXME: it seems that some systems like OS/2 requires PERL_SYS_INIT3
 * and PERL_SYS_TERM to open/close the same block, at least regarding some
 * ml messages.
 * 
 * Is passing @environ strictly needed ? --Zas */

#ifdef PERL_SYS_INIT3	/* this macro may not be defined, it depends on system. */
	char *my_argv[] = { NULL };
	int my_argc = 0;

	my_argv[my_argc++] = "";	/* hack to prevent unused variables warnings. */

	PERL_SYS_INIT3(&my_argc, &my_argv, &environ);
#endif

	my_perl = perl_alloc();
	if (my_perl) {
		char *hook_global = get_global_hook_file();
		char *hook_local = get_local_hook_file();
		char *global_argv[] = { "", hook_global};
		char *local_argv[] = { "", hook_local};
		int err = 1;

		perl_construct(my_perl);
		if (hook_local)
			err = perl_parse(my_perl, NULL, 2, local_argv, NULL);
		else if (hook_global)
			err = perl_parse(my_perl, NULL, 2, global_argv, NULL);
		PL_exit_flags |= PERL_EXIT_DESTRUCT_END;
		if (!err) err = perl_run(my_perl);
		if (err) precleanup_perl(module);
	}
}


struct module perl_scripting_module = struct_module(
	/* name: */		"Perl",
	/* options: */		NULL,
	/* hooks: */		perl_scripting_hooks,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		init_perl,
	/* done: */		cleanup_perl
);

#endif
