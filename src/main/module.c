/* General module system functionality */
/* $Id: module.c,v 1.16 2003/10/27 21:44:42 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "config/options.h"
#include "modules/module.h"


/* Dynamic area: */

#include "bfu/leds.h"
#include "cookies/cookies.h"
#include "bookmarks/bookmarks.h"
#include "formhist/formhist.h"
#include "globhist/globhist.h"
#include "mime/mime.h"
#include "scripting/scripting.h"

#ifdef HAVE_SSL
extern struct module ssl_module;
#endif

/* This is also used for version string composing so keep NULL terminated */
struct module *builtin_modules[] = {
#ifdef HAVE_SSL
	&ssl_module,
#endif
	&mime_module,
#ifdef USE_LEDS
	&leds_module,
#endif
#ifdef BOOKMARKS
	&bookmarks_module,
#endif
#ifdef COOKIES
	&cookies_module,
#endif
#ifdef FORMS_MEMORY
	&forms_history_module,
#endif
#ifdef GLOBHIST
	&global_history_module,
#endif
#ifdef HAVE_SCRIPTING
	&scripting_module,
#endif
	NULL
};

#define BUILTIN_MODULES_COUNT (sizeof(builtin_modules) / sizeof(struct module *) - 1)

/* Interface for handling single modules. */

void
register_module_options(struct module *module)
{
	if (module->options) register_options(module->options, config_options);

	if (module->submodules) {
		int i;

		for (i = 0; module->submodules[i]; i++)
			register_module_options(module->submodules[i]);
	}
}

void
unregister_module_options(struct module *module)
{
	/* First knock out the submodules if any. */
	if (module->submodules) {
		int i = 0;

		/* Cleanups backward to initialization. */
		while (module->submodules[i]) i++;

		for (i--; i >= 0; i--)
			unregister_module_options(module->submodules[i]);
	}

	if (module->options) unregister_options(module->options, config_options);
}

void
init_module(struct module *module)
{
	if (module->init) module->init(module);
	if (module->hooks) register_event_hooks(module->hooks);

	if (module->submodules) {
		int i;

		for (i = 0; module->submodules[i]; i++)
			init_module(module->submodules[i]);
	}
}

void
done_module(struct module *module)
{
	/* First knock out the submodules if any. */
	if (module->submodules) {
		int i = 0;

		/* Cleanups backward to initialization. */
		while (module->submodules[i]) i++;

		for (i--; i >= 0; i--)
			done_module(module->submodules[i]);
	}

	if (module->hooks) unregister_event_hooks(module->hooks);
	if (module->done) module->done(module);
}

/* Interface for handling builtin modules. */

void
register_modules_options(void)
{
	int i;

	for (i = 0; i < BUILTIN_MODULES_COUNT; i++)
		register_module_options(builtin_modules[i]);
}

void
unregister_modules_options(void)
{
	int i;

	/* Cleanups backward to initialization. */
	for (i = BUILTIN_MODULES_COUNT - 1; i >= 0; i--)
		unregister_module_options(builtin_modules[i]);
}

/* TODO: We probably need to have two builtin module tables one for critical
 * modules that should always be used and one for optional (the ones controlled
 * by init_b in main.c */

void
init_modules(void)
{
	int i;

	for (i = 0; i < BUILTIN_MODULES_COUNT; i++)
		init_module(builtin_modules[i]);
}

void
done_modules(void)
{
	int i;

	/* Cleanups backward to initialization. */
	for (i = BUILTIN_MODULES_COUNT - 1; i >= 0; i--)
		done_module(builtin_modules[i]);
}
