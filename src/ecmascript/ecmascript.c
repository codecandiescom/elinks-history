/* Base ECMAScript file. Mostly a proxy for specific library backends. */
/* $Id: ecmascript.c,v 1.13 2004/09/25 00:59:27 pasky Exp $ */

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
#include "sched/session.h"
#include "sched/task.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "viewer/text/view.h" /* current_frame() */
#include "viewer/text/vs.h"


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

	INIT_OPT_BOOL("ecmascript", N_("Script error reporting"),
		"error_reporting", 0, 0,
		N_("Open a message box when a script reports an error.")),

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

void
ecmascript_cleanup_state(struct document_view *doc_view, struct view_state *vs)
{
	if (ses->doc_view->ecmascript) {
		ecmascript_put_interpreter(ses->doc_view->ecmascript);
		ses->doc_view->ecmascript = NULL;
	}
	free_string_list(&vs->onload_snippets);
	vs->current_onload_snippet = NULL;
}


void
ecmascript_eval(struct ecmascript_interpreter *interpreter,
                struct string *code)
{
	if (!get_ecmascript_enable())
		return;
	assert(interpreter);
	spidermonkey_eval(interpreter, code);
}


unsigned char *
ecmascript_eval_stringback(struct ecmascript_interpreter *interpreter,
			   struct string *code)
{
	if (!get_ecmascript_enable())
		return NULL;
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
