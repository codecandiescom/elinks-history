/* Base ECMAScript file. Mostly a proxy for specific library backends. */
/* $Id: ecmascript.c,v 1.6 2004/09/23 14:03:54 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include "elinks.h"

#include "config/options.h"
#include "document/view.h"
#include "ecmascript/ecmascript.h"
#include "ecmascript/spidermonkey.h"
#include "intl/gettext/libintl.h"
#include "modules/module.h"
#include "protocol/uri.h"
#include "sched/task.h"
#include "viewer/text/view.h" /* current_frame() */


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


static void
ecmascript_init(struct module *module)
{
	spidermonkey_init();
}

static void
ecmascript_done(struct module *module)
{
	spidermonkey_done();
}


struct ecmascript_interpreter *
ecmascript_get_interpreter(struct document_view *doc_view)
{
	struct ecmascript_interpreter *interpreter;

	assert(doc_view);

	interpreter = mem_calloc(1, sizeof(struct ecmascript_interpreter));
	if (!interpreter)
		return NULL;

	interpreter->doc_view = doc_view;
	spidermonkey_get_interpreter(interpreter);

	return interpreter;
}

void
ecmascript_put_interpreter(struct ecmascript_interpreter *interpreter)
{
	assert(interpreter);
	spidermonkey_put_interpreter(interpreter);
	mem_free(interpreter);
}


unsigned char *
ecmascript_eval_stringback(struct ecmascript_interpreter *interpreter,
                           struct string *code)
{
	assert(interpreter);
	return spidermonkey_eval_stringback(interpreter, code);
}


void
ecmascript_protocol_handler(struct session *ses, struct uri *uri)
{
	struct document_view *doc_view = current_frame(ses);
	struct string current_url = INIT_STRING(struri(uri), strlen(struri(uri)));
	unsigned char *redirect_url;
	struct uri *redirect_uri;

	if (!doc_view)
		return;
	redirect_url = ecmascript_eval_stringback(doc_view->ecmascript,
		&current_url);
	if (!redirect_url)
		return;
	redirect_uri = get_hooked_uri(redirect_url, doc_view->session,
	                              doc_view->session->tab->term->cwd);
	mem_free(redirect_url);
	if (!redirect_uri)
		return;
	/* XXX: Is that safe to do at this point? --pasky */
	goto_uri_frame(ses, redirect_uri, doc_view->name,
		CACHE_MODE_NORMAL);
	done_uri(redirect_uri);
	return;
}


struct module ecmascript_module = struct_module(
	/* name: */		N_("ECMAScript"),
	/* options: */		ecmascript_options,
	/* events: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		ecmascript_init,
	/* done: */		ecmascript_done
);
