/* Guile interface (scripting engine) */
/* $Id: core.c,v 1.1 2003/07/24 15:33:33 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef HAVE_GUILE

#include <libguile.h>

#include "elinks.h"

#include "lowlevel/home.h"
#include "scripting/guile/core.h"
#include "util/error.h"
#include "util/string.h"


/*
 * Bindings
 */

/* static SCM c_current_url(void) */
/* { */
/* 	struct view_state *vs; */

/* 	if (have_location(ses) && (vs = ses ? &cur_loc(ses)->vs : 0)) */
/* 		return scm_makfrom0str(vs->url); */
/* 	else */
/* 		return SCM_BOOL_F; */
/* } */
/* c_current_link */
/* c_current_title */
/* c_current_document */
/* c_current_document_formatted */
/* c_bind_key */
/* c_xdialog */


/*
 * Console stuff
 */

/* TODO */


/*
 * Init
 */

void
init_guile(void)
{
	SCM user_module;
	SCM internal_module;
	unsigned char *path;

	scm_init_guile();

	/* Remember the current module. */
	user_module = scm_current_module();

	/* Load ~/.elinks/internal-hooks.scm. */
	path = straconcat(elinks_home, "internal-hooks.scm", NULL);
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
	scm_c_primitive_load_path(path);
	mem_free(path);
}



#endif /* HAVE_GUILE */
