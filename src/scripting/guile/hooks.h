/* $Id: hooks.h,v 1.2 2003/09/22 21:56:04 jonas Exp $ */

#ifndef EL__SCRIPTING_GUILE_HOOKS_H
#define EL__SCRIPTING_GUILE_HOOKS_H

#ifdef HAVE_GUILE

void register_guile_hooks(void);
void unregister_guile_hooks(void);

#endif

#endif
