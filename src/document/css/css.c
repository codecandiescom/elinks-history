/* CSS module management */
/* $Id: css.c,v 1.40 2004/04/02 21:21:59 jonas Exp $ */

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
#include "protocol/uri.h"
#include "util/error.h"
#include "util/memory.h"
#include "viewer/text/view.h"


struct option_info css_options_info[] = {
	INIT_OPT_TREE("document", N_("Cascading Style Sheets"),
		"css", OPT_SORT,
		N_("Options concerning how to use CSS for styling documents.")),

	INIT_OPT_BOOL("document.css", N_("Enable CSS"),
		"enable", 0, 1,
		N_("Enable adding of CSS style info to documents.")),

	INIT_OPT_BOOL("document.css", N_("Import external style sheets"),
		"import", 0, 1,
		N_("When enabled any external style sheets that are imported from\n"
		"either CSS itself using the @import keyword or from the HTML using\n"
		"<link> tags in the document header will also be downloaded.")),

	INIT_OPT_STRING("document.css", N_("Default style sheet"),
		"stylesheet", 0, "",
		N_("The path to the file containing the default user defined\n"
		"Cascading Style Sheet. It can be used to control the basic\n"
		"layout of HTML documents. It is assumed to be relative to\n"
		"ELinks' home directory.\n"
		"Leave as \"\" to use built-in document styling.")),

	NULL_OPTION_INFO,
};


void
import_css(struct css_stylesheet *css, unsigned char *url)
{
	/* Do we have it in the cache? (TODO: CSS cache) */
	struct uri *uri = get_uri(url, -1);
	struct cache_entry *cache_entry = uri ? find_in_cache(uri) : NULL;
	struct fragment *fragment;

	if (uri) done_uri(uri);
	if (!cache_entry
	    || css->import_level >= MAX_REDIRECTS)
		return;

	defrag_entry(cache_entry);
	fragment = cache_entry->frag.next;

	if (!list_empty(cache_entry->frag)
	    && !fragment->offset
	    && fragment->length) {
		unsigned char *end = fragment->data + fragment->length;

		css->import_level++;
		css_parse_stylesheet(css, fragment->data, end);
		css->import_level--;
	}
}


static void
import_css_file(struct css_stylesheet *css, unsigned char *url, int urllen)
{
	unsigned char filename[MAX_STR_LEN];
	struct string string;
	int length = 0;

	if (!*url
	    || css->import_level >= MAX_REDIRECTS)
		return;

	if (*url != '/' && elinks_home) {
		length = strlen(elinks_home);
		if (length > sizeof(filename)) return;
		safe_strncpy(filename, elinks_home, length + 1);
	}

	if (urllen + length > sizeof(filename)) return;

	safe_strncpy(filename + length, url, urllen + 1);
	length += urllen;

	if (read_encoded_file(filename, length, &string) == S_OK) {
		unsigned char *end = string.source + string.length;

		css->import_level++;
		css_parse_stylesheet(css, string.source, end);
		done_string(&string);
		css->import_level--;
	}
}

INIT_CSS_STYLESHEET(default_stylesheet, import_css_file);

static void
import_default_css(void)
{
	unsigned char *url = get_opt_str("document.css.stylesheet");

	if (!list_empty(default_stylesheet.selectors))
		done_css_stylesheet(&default_stylesheet);

	if (!*url) return;

	import_css_file(&default_stylesheet, url, strlen(url));
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
