/* General module system functionality */
/* $Id: module.c,v 1.2 2003/10/25 19:10:18 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "config/options.h"


/* Dynamic area: */

#include "bookmarks/bookmarks.h"
#include "mime/mime.h"

static struct module *builtin_modules[] = {
	&mime_module,
#ifdef BOOKMARKS
	&bookmarks_module,
#endif
	NULL,
};


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

	if (module->done) module->done(module);
}

/* Interface for handling builtin modules. */

void
register_modules_options(void)
{
	int i;

	for (i = 0; builtin_modules[i]; i++)
		register_module_options(builtin_modules[i]);
}

void
unregister_modules_options(void)
{
	int i = 0;

	/* Cleanups backward to initialization. */
	/* TODO: We can probably figure out the number of modules by looking at
	 *	 sizeof(builtin_modules). --jonas */
	while (builtin_modules[i]) i++;

	for (i--; i >= 0; i--)
		unregister_module_options(builtin_modules[i]);
}

/* TODO: We probably need to have two builtin module tables one for critical
 * modules that should always be used and one for optional (the ones controlled
 * by init_b in main.c */

void
init_modules(void)
{
	int i;

	for (i = 0; builtin_modules[i]; i++)
		init_module(builtin_modules[i]);
}

void
done_modules(void)
{
	int i = 0;

	/* Cleanups backward to initialization. */
	/* TODO: We can probably figure out the number of modules by looking at
	 *	 sizeof(builtin_modules). --jonas */
	while (builtin_modules[i]) i++;

	for (i--; i >= 0; i--)
		done_module(builtin_modules[i]);
}
