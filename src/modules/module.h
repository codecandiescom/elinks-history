/* $Id: module.h,v 1.1 2003/10/25 16:38:43 jonas Exp $ */

#ifndef EL__MODULES_MODULE_H
#define EL__MODULES_MODULE_H

struct module {
	/* The (unique) name of the module. */
	unsigned char *name;

	/* The options that should be registered for this module.
	 * The table should end with NULL_OPTION_INFO. */
	struct option_info *options;

	/* Any submodules that this module contains. Order matters
	 * since it is garanteed that initialization will happen in
	 * the specified order and teardown in the reverse order.
	 * The table should end with NULL. */
	struct module **submodules;

	/* Lifecycle functions. */
	void (*init)(struct module *module);
	void (*done)(struct module *module);
};

#define INIT_MODULE(name, options, submods, init, done) \
	{ name, options, submods, init, done }

/* Interface for handling single modules. */

void register_module_options(struct module *module);
void unregister_module_options(struct module *module);

void init_module(struct module *module);
void done_module(struct module *module);

/* Interface for handling builtin modules. */

void register_modules_options(void);
void unregister_modules_options(void);

void init_modules(void);
void done_modules(void);

#endif
