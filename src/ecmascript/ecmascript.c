/* Base ECMAScript file. Mostly a proxy for specific library backends. */
/* $Id: ecmascript.c,v 1.1 2004/09/21 22:11:43 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include "config/options.h"
#include "document/view.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/njs.h"
#include "intl/gettext/libintl.h"
#include "modules/module.h"


enum ecmascript_option {
	ECMASCRIPT_TREE,

	ECMASCRIPT_ENABLE,

	ECMASCRIPT_OPTIONS,
};

static struct option_info ecmascript_options[] = {
	INIT_OPT_TREE("", N_("ECMAScript"),
		"ecmascript", 0,
		N_("ECMAScript options.")),

	INIT_OPT_BOOL("ecmascript", N_("Enable"),
		"enable", 0, 1,
		N_("Whether to run those scripts inside of documents.")),

	NULL_OPTION_INFO,
};

#define get_opt_ecmascript(which)	ecmascript_options[(which)].option.value
#define get_ecmascript_enable()		get_opt_ecmascript(ECMASCRIPT_ENABLE).number

struct ecmascript_interpreter *
ecmascript_get_interpreter(struct document_view *doc_view)
{
	struct ecmascript_interpreter *interpreter;

	assert(doc_view);

	interpreter = mem_calloc(1, sizeof(struct ecmascript_interpreter));
	if (!interpreter)
		return NULL;

	interpreter->doc_view = doc_view;
	njs_get_interpreter(interpreter);

	return interpreter;
}

void
ecmascript_put_interpreter(struct ecmascript_interpreter *interpreter)
{
	assert(interpreter);
	njs_put_interpreter(interpreter);
	mem_free(interpreter);
}

struct module ecmascript_module = struct_module(
	/* name: */		N_("ECMAScript"),
	/* options: */		ecmascript_options,
	/* events: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		NULL,
	/* done: */		NULL
);
