/* CSS module management */
/* $Id: css.c,v 1.32 2004/01/25 02:10:31 jonas Exp $ */

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
#include "protocol/file/file.h"
#include "util/error.h"
#include "util/memory.h"
#include "viewer/text/view.h"


INIT_CSS_STYLESHEET(default_stylesheet);

struct option_info css_options_info[] = {
	INIT_OPT_TREE("document", N_("Cascading Style Sheets"),
		"css", 0,
		N_("Options concerning how to use CSS for styling documents.")),

	INIT_OPT_BOOL("document.css", N_("Enable CSS"),
		"enable", 0, 1,
		N_("Enable adding of CSS style info to documents.")),

	INIT_OPT_STRING("document.css", N_("Default style sheet"),
		"stylesheet", 0, "",
		N_("The path to the file containing the default user defined\n"
		"Cascading Style Sheet. It can be used to control the basic\n"
		"layout of HTML documents. It is assumed to be relative to\n"
		"ELinks' home directory.\n"
		"Leave as \"\" to use built-in document styling.")),

	NULL_OPTION_INFO,
};


static void
import_css_file(struct css_stylesheet *css, unsigned char *url)
{
	unsigned char *home_url = NULL;
	struct string string;

	if (!*url) return;

	if (*url != '/' && elinks_home) {
		home_url = straconcat(elinks_home, url, NULL);
		if (!home_url) return;
		url = home_url;
	}

	if (read_encoded_file(url, strlen(url), &string) == S_OK) {
		css_parse_stylesheet(css, string.source);
		done_string(&string);
	}
	if (home_url) mem_free(home_url);
}

static void
import_default_css(void)
{
	unsigned char *url = get_opt_str("document.css.stylesheet");
	struct string_list_item *import;

	if (!list_empty(default_stylesheet.selectors))
		done_css_stylesheet(&default_stylesheet);

	if (!*url) return;

	import_css_file(&default_stylesheet, url);

	if (list_empty(default_stylesheet.imports)) return;

	/* Hmm .. ok we start over */
	while (!list_empty(default_stylesheet.selectors)) {
		struct css_selector *selector = default_stylesheet.selectors.next;

		del_from_list(selector);
		free_list(selector->properties);
		mem_free(selector->element);
		mem_free(selector);
	}

	foreach (import, default_stylesheet.imports) {
		import_css_file(&default_stylesheet, import->string.source);
	}

	import_css_file(&default_stylesheet, url);
}

static int
change_hook_css(struct session *ses, struct option *current, struct option *changed)
{
	if (!strcmp(changed->name, "stylesheet")) {
		/* TODO: We need to update all entries in format cache. --jonas */
		import_default_css();
	}

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
