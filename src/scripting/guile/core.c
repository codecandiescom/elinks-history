/* Guile interface (scripting engine) */
/* $Id: core.c,v 1.13 2004/04/01 14:35:24 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_GUILE

#include <libguile.h>

#include "elinks.h"

#include "lowlevel/home.h"
#include "modules/module.h"
#include "scripting/guile/core.h"
#include "scripting/guile/hooks.h"
#include "scripting/scripting.h"
#include "util/error.h"
#include "util/string.h"


/*
 * Bindings
 */

/* static SCM c_current_url(void) */
/* { */
/* 	struct view_state *vs; */

/* 	if (have_location(ses) && (vs = ses ? &cur_loc(ses)->vs : 0)) */
/* 		return scm_makfrom0str(struri(vs->uri)); */
/* 	else */
/* 		return SCM_BOOL_F; */
/* } */
/* c_current_link */
/* c_current_title */
/* c_current_document */
/* c_current_document_formatted */
/* c_bind_key */
/* c_xdialog */


static void
init_guile(struct module *module)
{
	SCM user_module;
	SCM internal_module;
	unsigned char *path;

	scm_init_guile();

	if (!elinks_home) return;

	/* Remember the current module. */
	user_module = scm_current_module();

	/* Load ~/.elinks/internal-hooks.scm. */
	path = straconcat(elinks_home, "internal-hooks.scm", NULL);
	if (!path) return;
	scm_c_primitive_load_path(path);
	mem_free(path);

	/* internal-hooks.scm should have created a new module (elinks
	 * internal).  Let's remember it, even though I haven't
	 * figured out how to use it directly yet...
	 */
	internal_module = scm_current_module();

	/* Return to the user module, import bindings from (elinks
	 * internal), then load ~/.elinks/user-hooks.scm.
	 */
	scm_set_current_module(user_module);
	scm_c_use_module("elinks internal"); /* XXX: better way? i want to use internal_module directly */
	path = straconcat(elinks_home, "user-hooks.scm", NULL);
	if (!path) return;
	scm_c_primitive_load_path(path);
	mem_free(path);
}


struct module guile_scripting_module = struct_module(
	/* name: */		"Guile",
	/* options: */		NULL,
	/* events: */		guile_scripting_hooks,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		init_guile,
	/* done: */		NULL
);

#endif /* HAVE_GUILE */
