/* $Id: scripting.h,v 1.1 2003/09/23 00:45:18 jonas Exp $ */

#ifndef EL__SCRIPTING_SCRIPTING_H
#define EL__SCRIPTING_SCRIPTING_H

#ifdef HAVE_SCRIPTING

#include <stdarg.h>

struct scripting_hook {
	unsigned char *name;
	int (*callback)(va_list ap);
};

/* Plugs hooks into the event system. */
/* XXX: Last entry of @hooks must have a NULL @name. */
void register_scripting_hooks(struct scripting_hook *hooks);
void unregister_scripting_hooks(struct scripting_hook *hooks);

#endif

#endif
