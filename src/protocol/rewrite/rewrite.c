/* URI rewriting module */
/* $Id: rewrite.c,v 1.7 2003/12/08 23:08:27 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifdef URI_REWRITE

#include "elinks.h"

#include "config/options.h"
#include "intl/gettext/libintl.h"
#include "modules/module.h"
#include "protocol/rewrite/rewrite.h"
#include "protocol/uri.h"
#include "sched/event.h"
#include "sched/location.h"
#include "sched/session.h"
#include "util/string.h"


/* TODO: An event hook for follow-url might also feel at home here. --jonas */

enum uri_rewrite_option {
	URI_REWRITE_TREE,

	URI_REWRITE_ENABLE_DUMB,
	URI_REWRITE_ENABLE_SMART,

	URI_REWRITE_DUMB_TREE,
	URI_REWRITE_DUMB_TEMPLATE,

	URI_REWRITE_SMART_TREE,
	URI_REWRITE_SMART_TEMPLATE,

	URI_REWRITE_OPTIONS,
};

static struct option_info uri_rewrite_options[] = {
	INIT_OPT_TREE("protocol", N_("URI rewriting"),
		"rewrite", OPT_SORT,
		N_("Rules for rewriting URIs entered in the goto dialog.\n"
		"It makes it possible to define a set of prefixes that will\n"
		"be expanded if they match a string entered in the goto dialog.\n"
		"The prefixes can be dumb, meaning that they work only like\n"
		"URI abbreviations, or smart ones, making it possible to pass\n"
		"arguments to them like search engine keywords.")),

	INIT_OPT_BOOL("protocol.rewrite", N_("Enable dumb prefixes"),
		"enable-dumb", 0, 1,
		N_("Enable dumb prefixes.")),

	INIT_OPT_BOOL("protocol.rewrite", N_("Enable smart prefixes"),
		"enable-smart", 0, 1,
		N_("Enable smart prefixes.")),

	INIT_OPT_TREE("protocol.rewrite", N_("Dumb Prefixes"),
		"dumb", OPT_AUTOCREATE,
		N_("Dumb prefixes.")),

	INIT_OPT_STRING("protocol.rewrite.dumb", NULL,
		"_template_", 0, "",
		N_("Replacement URI for this dumbprefix.\n"
		"%c in the string means the current URL\n"
		"%% in the string means '%'")),

	INIT_OPT_TREE("protocol.rewrite", N_("Smart Prefixes"),
		"smart", OPT_AUTOCREATE,
		N_("Smart prefixes.")),

	/* TODO: In some rare occations current link URI and referrer might
	 * also be useful and dare I mention some kind of proxy argument. --jonas */
	INIT_OPT_STRING("protocol.rewrite.smart", NULL,
		"_template_", 0, "",
		N_("Replacement URI for this smartprefix.\n"
		"%c in the string means the current URL\n"
		"%s in the string means the whole argument to smartprefix\n"
		"%0,%1,...,%9 means argument 0, 1, ..., 9\n"
		"%% in the string means '%'")),

#define INIT_OPT_DUMB_PREFIX(prefix, uri) \
	INIT_OPT_STRING("protocol.rewrite.dumb", NULL, prefix, 0, uri, NULL)

	INIT_OPT_DUMB_PREFIX("elinks", "http://elinks.or.cz/"),
	INIT_OPT_DUMB_PREFIX("documentation", "http://elinks.or.cz/documentation/"),

#define INIT_OPT_SMART_PREFIX(prefix, uri) \
	INIT_OPT_STRING("protocol.rewrite.smart", NULL, prefix, 0, uri, NULL)

	INIT_OPT_SMART_PREFIX("bug", "http://bugzilla.elinks.or.cz/show_bug.cgi?id=%s"),
	INIT_OPT_SMART_PREFIX("google", "http://www.google.com/search?q=%s"),

	NULL_OPTION_INFO,
};

#define get_opt_rewrite(which)	uri_rewrite_options[(which)].option
#define get_dumb_enable()	get_opt_rewrite(URI_REWRITE_ENABLE_DUMB).value.number
#define get_smart_enable()	get_opt_rewrite(URI_REWRITE_ENABLE_SMART).value.number

static inline struct option *
get_prefix_tree(enum uri_rewrite_option tree)
{
	assert(tree == URI_REWRITE_DUMB_TREE
	       || tree == URI_REWRITE_SMART_TREE);
	return &get_opt_rewrite(tree);
}

static inline void
encode_uri_string_len(struct string *s, unsigned char *a, int alen)
{
	unsigned char c = a[alen];

	a[alen] = 0;
	encode_uri_string(s, a);
	a[alen] = c;
}

#define MAX_URI_ARGS 10

static unsigned char *
substitute_url(unsigned char *url, unsigned char *current_url, unsigned char *arg)
{
	struct string n = NULL_STRING;
	unsigned char *args[MAX_URI_ARGS];
	int argslen[MAX_URI_ARGS];
	int argc = 0;
	int i;

	if (!init_string(&n)) return NULL;

	/* Extract space separated list of arguments */
	args[argc] = arg;
	for (i = 0; ; i++) {
		if (args[argc][i] == ' ') {
			argslen[argc] = i;
			argc++;
			if (argc == MAX_URI_ARGS) break;
			args[argc] = &args[argc - 1][i];
			i = 0;
			for (; *args[argc] && *args[argc] == ' '; args[argc]++);
		} else if (!args[argc][i]) {
			argslen[argc] = i;
			argc++;
			break;
		}
	}

	while (*url) {
		int p;
		int value;

		for (p = 0; url[p] && url[p] != '%'; p++);

		add_bytes_to_string(&n, url, p);
		url += p;

		if (*url != '%') continue;

		url++;
		switch (*url) {
			case 'c':
				add_to_string(&n, current_url);
				break;
			case 's':
				if (arg) encode_uri_string(&n, arg);
				break;
			case '%':
				add_char_to_string(&n, '%');
				break;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				value = *url - '0';
				if (value < argc)
					encode_uri_string_len(&n, args[value],
						argslen[value]);
				break;
			default:
				add_bytes_to_string(&n, url - 1, 2);
				break;
		}
		if (*url) url++;
	}

	return n.source;
}

static unsigned char *
get_prefix(enum uri_rewrite_option tree, unsigned char *url)
{
	struct option *prefix_tree = get_prefix_tree(tree);
	struct option *opt = get_opt_rec_real(prefix_tree, url);
	unsigned char *exp = opt ? opt->value.string : NULL;

	return (exp && *exp) ? exp : NULL;
}

static enum evhook_status
goto_url_hook(va_list ap, void *data)
{
	unsigned char **url = va_arg(ap, unsigned char **);
	struct session *ses = va_arg(ap, struct session *);
	unsigned char *uu = NULL;
	unsigned char *arg = "";

	if (get_dumb_enable())
		uu = get_prefix(URI_REWRITE_DUMB_TREE, *url);

	if (!uu && get_smart_enable()) {
		unsigned char *argstart = *url + strcspn(*url, " :");

		if (*argstart) {
			unsigned char bucket = *argstart;

			*argstart = '\0';
			uu = get_prefix(URI_REWRITE_SMART_TREE, *url);
			*argstart = bucket;
			arg = argstart + 1;
		}
	}

	if (uu) {
		uu = substitute_url(uu, cur_loc(ses)->vs.url, arg);
		if (uu) *url = uu;
	}

	return EHS_NEXT;
}

struct event_hook_info uri_rewrite_hooks[] = {
	{ "goto-url",	goto_url_hook },

	NULL_EVENT_HOOK_INFO
};

struct module uri_rewrite_module = struct_module(
	/* name: */		N_("URI rewrite"),
	/* options: */		uri_rewrite_options,
	/* hooks: */		uri_rewrite_hooks,
	/* submodules: */	NULL,
	/* data: */		NULL,
	/* init: */		NULL,
	/* done: */		NULL
);

#endif
