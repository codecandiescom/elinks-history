/* CSS module management */
/* $Id: css.c,v 1.27 2004/01/24 19:57:10 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include "elinks.h"

#include "config/options.h"
#include "document/css/css.h"
#include "document/css/parser.h"
#include "document/css/stylesheet.h"
#include "intl/gettext/libintl.h"
#include "lowlevel/home.h"
#include "modules/module.h"
#include "protocol/protocol.h"
#include "sched/connection.h"
#include "terminal/terminal.h"
#include "util/error.h"
#include "util/memory.h"
#include "viewer/text/view.h"


struct css_import {
	struct css_stylesheet *css;
	struct download download;
	int redir_count;
};

struct css_stylesheet default_stylesheet = {
	{ D_LIST_HEAD(default_stylesheet.selectors) },
};

struct option_info css_options_info[] = {
	INIT_OPT_TREE("document", N_("Cascading Style Sheets"),
		"css", 0,
		N_("Options concerning how to use CSS for styling documents.")),

	INIT_OPT_BOOL("document.css", N_("Enable CSS"),
		"enable", 0, 1,
		N_("Enable adding of CSS style info to documents.")),

	INIT_OPT_STRING("document.css", N_("Default style sheet"),
		"stylesheet", 0, "",
		N_("The URI for the default Cascading Style Sheet. It can be\n"
		"used to control the basic layout of HTML documents. It is\n"
		"assumed to be relative to ELinks' home directory.\n"
		"Leave as \"\" to use built-in document styling.")),

	NULL_OPTION_INFO,
};


/* Much code for importing was stolen from dump_(start|end) */
static void
import_css_end(struct download *download, void *css_import)
{
	struct css_import *import = css_import;
	struct cache_entry *cache_entry = download->ce;
	struct fragment *fragment;

	assert(import);

	if (cache_entry
	    && cache_entry->redirect
	    && import->redir_count++ < MAX_REDIRECTS) {
		unsigned char *cache_uri = get_cache_uri(cache_entry);
		unsigned char *url;

		if (download->state >= 0)
			change_connection(download, NULL, PRI_CANCEL, 0);

		url = join_urls(cache_uri, cache_entry->redirect);
		if (!url) {
			mem_free(import);
			return;
		}

		if (load_url(url, cache_uri, download, PRI_MAIN, 0, -1))
			mem_free(import);
		mem_free(url);
		return;
	}

	if (download->state >= 0 && download->state < S_TRANS)
		return;

	if (!cache_entry) {
		mem_free(import);
		return;
	}

	defrag_entry(cache_entry);
	fragment= cache_entry->frag.next;
	if (fragment != (void *) &cache_entry->frag
	    && !fragment->offset
	    && fragment->length) {
		css_parse_stylesheet(import->css, fragment->data);
	}

	mem_free(import);
}

static void
import_css(struct css_stylesheet *css, unsigned char *url)
{
	struct css_import *import;
	unsigned char *real_url = NULL;
	unsigned char *wd;

	assert(css);

	if (!*url) return;

	import = mem_calloc(1, sizeof(struct css_import));
	if (!import) return;

	import->css = css;
	import->download.end = import_css_end;
	import->download.data = import;

	wd = get_cwd();
	real_url = translate_url(url, wd);
	if (wd) mem_free(wd);

	if (!real_url) real_url = stracpy(url);
	if (!real_url
	    || known_protocol(real_url, NULL) == PROTOCOL_UNKNOWN
	    || load_url(real_url, NULL, &import->download, PRI_MAIN, 0, -1)) {
		mem_free(import);
	}

	if (real_url) mem_free(real_url);
}


static void
import_default_css(void)
{
	unsigned char *url = get_opt_str("document.css.stylesheet");
	unsigned char *home_url = NULL;

	if (!list_empty(default_stylesheet.selectors))
		done_css_stylesheet(&default_stylesheet);

	if (!*url) return;

	if (*url != '/' && elinks_home) {
		home_url = straconcat(elinks_home, url, NULL);
		if (!home_url) return;
		url = home_url;
	}

	import_css(&default_stylesheet, url);
	if (home_url) mem_free(home_url);
}

static int
change_hook_css(struct session *ses, struct option *current, struct option *changed)
{
	if (!strcmp(changed->name, "stylesheet"))
		import_default_css();

	draw_formatted(ses, 1);

	return 0;
}

static void
init_css(struct module *module)
{
	struct change_hook_info css_change_hooks[] = {
		{ "document.css",		change_hook_css },
		{ NULL,				NULL },
	};

	register_change_hooks(css_change_hooks);
	import_default_css();
}

void
done_css(struct module *module)
{
	done_css_stylesheet(&default_stylesheet);
}


struct module css_module = struct_module(
	/* name: */		N_("Cascading Style Sheets"),
	/* options: */		css_options_info,
	/* hooks: */		NULL,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		init_css,
	/* done: */		done_css
);
