/* $Id: module.h,v 1.5 2003/10/25 21:46:19 pasky Exp $ */

#ifndef EL__MODULES_MODULE_H
#define EL__MODULES_MODULE_H

/* The module record */

struct module {
	/* The name of the module. It needs to be unique in its class (ie. in
	 * the scope of root modules or submodules of one parent module). */
	unsigned char *name;

	/* The options that should be registered for this module.
	 * The table should end with NULL_OPTION_INFO. */
	struct option_info *options;

	/* Any submodules that this module contains. Order matters
	 * since it is garanteed that initialization will happen in
	 * the specified order and teardown in the reverse order.
	 * The table should end with NULL. */
	struct module **submodules;

	/* User data for the module. Undefined purpose. */
	void *data;

	/* Lifecycle functions */

	/* This function should initialize the module. */
	void (*init)(struct module *module);

	/* This function should shutdown the module. */
	void (*done)(struct module *module);
};

#define INIT_MODULE(name, options, submods, init, done) \
	{ name, options, submods, init, done }

/* Interface for handling single modules */

void register_module_options(struct module *module);
void unregister_module_options(struct module *module);

void init_module(struct module *module);
void done_module(struct module *module);

/* Interface for handling builtin modules */

void register_modules_options(void);
void unregister_modules_options(void);

void init_modules(void);
void done_modules(void);

#endif
