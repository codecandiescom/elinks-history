/* Options variables manipulation core */
/* $Id: options.c,v 1.362 2003/10/25 12:33:45 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h> /* OS/2 needs this after sys/types.h */
#include <sys/stat.h> /* OS/2 needs this after sys/types.h */
#include <netdb.h>

/* We need to have it here. Stupid BSD. */
#include <netinet/in.h>
#ifdef HAVE_ARPA_INET_H
#include <arpa/inet.h>
#endif

#include "elinks.h"

#include "main.h" /* shrink_memory() */
#include "bfu/listbox.h"
#include "config/conf.h"
#include "config/options.h"
#include "config/opttypes.h"
#include "document/cache.h"
#include "document/html/parser.h"
#include "globhist/globhist.h"
#include "intl/charsets.h"
#include "intl/gettext/libintl.h"
#include "lowlevel/dns.h"
#include "lowlevel/select.h"
#include "sched/session.h"
#include "util/color.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/view.h"

/* TODO? In the past, covered by shadow and legends, remembered only by the
 * ELinks Elders now, options were in hashes (it was not for a long time, after
 * we started to use dynamic options lists and before we really started to use
 * hierarchic options). Hashes might be swift and deft, but they had a flaw and
 * the flaw showed up as the fatal flaw. They were unsorted, and it was
 * unfriendly to mere mortal users, without pasky's options handlers in their
 * brain, but their own poor-written software. And thus pasky went and rewrote
 * options so that they were in lists from then to now and for all the ages of
 * men, to the glory of mankind. However, one true hero may arise in future
 * fabulous and implement possibility to have both lists and hashes for trees,
 * as it may be useful for some supernatural entities. And when that age will
 * come... */

/* TODO: We should remove special case for root options and use some auxiliary
 * (struct option *) instead. This applies to bookmarks, global history and
 * listbox items as well, though. --pasky */

static INIT_LIST_HEAD(options_root_tree);

static struct option options_root = INIT_OPTION(
	/* name: */	"",
	/* flags: */	0,
	/* type: */	OPT_TREE,
	/* min, max: */	0, 0,
	/* value: */	&options_root_tree,
	/* desc: */	"",
	/* capt: */	NULL
);

struct option *config_options;
struct option *cmdline_options;

INIT_LIST_HEAD(config_option_box_items);
INIT_LIST_HEAD(option_boxes);

static void add_opt_rec(struct option *, unsigned char *, struct option *);
static void free_options_tree(struct list_head *);

/**********************************************************************
 Options interface
**********************************************************************/

/* If option name contains dots, they are created as "categories" - first,
 * first category is retrieved from list, taken as a list, second category
 * is retrieved etc. */

/* Ugly kludge */
static int no_autocreate = 0;

/* Get record of option of given name, or NULL if there's no such option. */
struct option *
get_opt_rec(struct option *tree, unsigned char *name_)
{
	struct option *option;
	unsigned char *aname = stracpy(name_);
	unsigned char *name = aname;
	unsigned char *sep;

	if (!aname) return NULL;

	/* We iteratively call get_opt_rec() each for path_elements-1, getting
	 * appropriate tree for it and then resolving [path_elements]. */
	if ((sep = strrchr(name, '.'))) {
		*sep = '\0';

		tree = get_opt_rec(tree, name);
		if (!tree || tree->type != OPT_TREE || tree->flags & OPT_HIDDEN) {
#if 0
			debug("ERROR in get_opt_rec() crawl: %s (%d) -> %s",
			      name, tree ? tree->type : -1, sep + 1);
#endif
			mem_free(aname);
			return NULL;
		}

		*sep = '.';
		name = sep + 1;
	}

	foreach (option, *tree->value.tree) {
		if (option->name && !strcmp(option->name, name)) {
			mem_free(aname);
			return option;
		}
	}

	if (tree && tree->flags & OPT_AUTOCREATE && !no_autocreate) {
		struct option *template = get_opt_rec(tree, "_template_");

		assertm(template, "Requested %s should be autocreated but "
			"%.*s._template_ is missing!", name_, sep - name_,
			name_);
		if_assert_failed {
			mem_free(aname);
			return NULL;
		}

		/* We will just create the option and return pointer to it
		 * automagically. And, we will create it by cloning _template_
		 * option. By having _template_ OPT_AUTOCREATE and _template_
		 * inside, you can have even multi-level autocreating. */

		option = copy_option(template);
		if (!option) {
			mem_free(aname);
			return NULL;
		}
		if (option->name) mem_free(option->name);
		option->name = stracpy(name);
		if (option->box_item) option->box_item->text = option->name;

		add_opt_rec(tree, "", option);

		mem_free(aname);
		return option;
	}

	mem_free(aname);
	return NULL;
}

/* Get record of option of given name, or NULL if there's no such option. But
 * do not create the option if it doesn't exist and there's autocreation
 * enabled. */
struct option *
get_opt_rec_real(struct option *tree, unsigned char *name)
{
	struct option *opt;

	no_autocreate = 1;
	opt = get_opt_rec(tree, name);
	no_autocreate = 0;
	return opt;
}

/* Fetch pointer to value of certain option. It is guaranteed to never return
 * NULL. Note that you are supposed to use wrapper get_opt(). */
union option_value *
get_opt_(
#ifdef DEBUG
	 unsigned char *file, int line,
#endif
	 struct option *tree, unsigned char *name)
{
	struct option *opt = get_opt_rec(tree, name);

#ifdef DEBUG
	errfile = file;
	errline = line;
	if (!opt) elinks_internal("Attempted to fetch nonexisting option %s!", name);

	/* Various sanity checks. */
	switch (opt->type) {
	case OPT_TREE:
		if (!opt->value.tree)
			elinks_internal("Option %s has no value!", name);
		break;
	case OPT_STRING:
	case OPT_ALIAS:
		if (!opt->value.string)
			elinks_internal("Option %s has no value!", name);
		break;
	case OPT_BOOL:
	case OPT_INT:
	case OPT_LONG:
		if (opt->value.number < opt->min
		    || opt->value.number > opt->max)
			elinks_internal("Option %s has invalid value!", name);
		break;
	case OPT_COMMAND:
		if (!opt->value.command)
			elinks_internal("Option %s has no value!", name);
		break;
	default:
		break;
	}
#endif

	return &opt->value;
}

/* Add option to tree. */
static void
add_opt_rec(struct option *tree, unsigned char *path, struct option *option)
{
	struct list_head *cat = tree->value.tree;

	if (*path) {
		tree = get_opt_rec(tree, path);
		cat = tree->value.tree;
	}
	if (!cat) return;

	if (option->box_item && option->name && !strcmp(option->name, "_template_"))
		option->box_item->visible = get_opt_int("config.show_template");

	if (tree->box_item && option->box_item) {
		option->box_item->depth = tree->box_item->depth + 1;
		option->box_item->root = tree->box_item;

		add_to_list_end(tree->box_item->child, option->box_item);
	} else if (option->box_item) {
		add_to_list_end(config_option_box_items, option->box_item);
	}

#if 0
	if (tree->flags & OPT_SORT) {
		struct option *pos;

		/* The list is empty, just add it there. */
		if (list_empty(*cat)) {
			add_to_list(*cat, option);

		/* This fits as the last list entry, add it there. This
		 * optimizes the most expensive BUT most common case ;-). */
		} else if (strcmp((pos = cat->prev)->name, option->name) <= 0) {
			add_to_list_end(*cat, option);

		/* Scan the list linearly. This could be probably optimized ie.
		 * to choose direction based on the first letter or so. */
		} else {
			foreach (pos, *cat) {
				if (strcmp(pos->name, option->name) <= 0)
					continue;

				add_at_pos(pos->prev, option);
				break;
			}

			assert(pos != (struct option *) cat);
		}

	} else
#endif
	{
		add_at_pos((struct option *) cat->prev, option);
	}
}

static inline struct listbox_item *
init_option_listbox_item(struct option *option)
{
	struct listbox_item *box = mem_calloc(1, sizeof(struct listbox_item));

	if (box) {
		init_list(box->child);
		box->visible = 1;
		box->translated = 1;
		box->text = option->capt ? option->capt : option->name;
		box->box = &option_boxes;
		box->udata = option;
		box->type = (option->type == OPT_TREE) ? BI_FOLDER : BI_LEAF;
	}

	return box;
}

struct option *
add_opt(struct option *tree, unsigned char *path, unsigned char *capt,
	unsigned char *name, enum option_flags flags, enum option_type type,
	int min, int max, void *value, unsigned char *desc)
{
	struct option *option = mem_calloc(1, sizeof(struct option));

	if (!option) return NULL;

	option->name = stracpy(name);
	if (!option->name) {
		mem_free(option);
		return NULL;
	}
	option->flags = (flags | OPT_ALLOC);
	option->type = type;
	option->min = min;
	option->max = max;
	option->capt = capt;
	option->desc = desc;

	if (option->type != OPT_ALIAS && (tree->flags & OPT_LISTBOX)) {
		option->box_item = init_option_listbox_item(option);
		if (!option->box_item) {
			delete_option(option);
			return NULL;
		}
	}

	/* XXX: For allocated values we allocate in the add_opt_<type>() macro.
	 * This involves OPT_TREE and OPT_STRING. */
	switch (type) {
		case OPT_TREE:
			if (!value) {
				delete_option(option);
				return NULL;
			}
			option->value.tree = value;
			break;
		case OPT_STRING:
			if (!value) {
				delete_option(option);
				return NULL;
			}
			option->value.string = value;
			break;
		case OPT_ALIAS:
			option->value.string = value;
			break;
		case OPT_BOOL:
		case OPT_INT:
		case OPT_CODEPAGE:
		case OPT_LONG:
			option->value.number = (long) value;
			break;
		case OPT_COLOR:
			decode_color(value, &option->value.color);
			break;
		case OPT_COMMAND:
			option->value.command = value;
			break;
		case OPT_LANGUAGE:
			break;
		default:
			/* XXX: Make sure all option types are handled here. */
			internal("Invalid option type %d", type);
			break;
	}

	add_opt_rec(tree, path, option);
	return option;
}

/* The namespace may start to seem a bit chaotic here; it indeed is, maybe the
 * function names above should be renamed and only macros should keep their old
 * short names. */
/* The simple rule I took as an apologize is that functions which take already
 * completely filled (struct option *) have long name and functions which take
 * only option specs have short name. */

void
delete_option(struct option *option)
{
	if (option->next) del_from_list(option);

	switch (option->type) {
		case OPT_STRING:
			if (option->value.string)
				mem_free(option->value.string);
			break;
		case OPT_TREE:
			if (option->value.tree) {
				free_options_tree(option->value.tree);
				mem_free(option->value.tree);
			}
			break;
		default:
			break;
	}

	if (option->box_item) {
		del_from_list(option->box_item);
		mem_free(option->box_item);
	}

	if (option->flags & OPT_ALLOC) {
		if (option->name) mem_free(option->name);
		mem_free(option);
	}
}

struct option *
copy_option(struct option *template)
{
	struct option *option = mem_calloc(1, sizeof(struct option));

	if (!option) return NULL;

	option->name = template->name ? stracpy(template->name) : NULL;
	option->flags = (template->flags | OPT_ALLOC);
	option->type = template->type;
	option->min = template->min;
	option->max = template->max;
	option->capt = template->capt;
	option->desc = template->desc;
	option->change_hook = template->change_hook;

	option->box_item = init_option_listbox_item(template);
	if (option->box_item) {
		if (template->box_item) {
			option->box_item->type = template->box_item->type;
			option->box_item->depth = template->box_item->depth;
		}
		option->box_item->udata = option;
	}

	if (option_types[template->type].dup) {
		option_types[template->type].dup(option, template);
	} else {
		option->value = template->value;
	}

	return option;
}


static void register_options(void);
static void unregister_options(void);

static struct change_hook_info change_hooks[];

struct list_head *
init_options_tree(void)
{
	struct list_head *ptr = mem_alloc(sizeof(struct list_head));

	if (ptr) init_list(*ptr);
	return ptr;
}

/* Some default pre-autocreated options. Doh. */
static inline void
register_autocreated_options(void)
{
	static const unsigned char image_gif[]  = "image/gif";
	static const unsigned char image_jpeg[] = "image/jpeg";
	static const unsigned char image_png[]  = "image/png";
	static const unsigned char text_plain[] = "text/plain";
	static const unsigned char text_html[]  = "text/html";

	static const unsigned char mailto[] = DEFAULT_AC_OPT_MAILTO;
	static const unsigned char telnet[] = DEFAULT_AC_OPT_TELNET;
	static const unsigned char tn3270[] = DEFAULT_AC_OPT_TN3270;
	static const unsigned char gopher[] = DEFAULT_AC_OPT_GOPHER;
	static const unsigned char news[]   = DEFAULT_AC_OPT_NEWS;
	static const unsigned char irc[]    = DEFAULT_AC_OPT_IRC;

	/* TODO: Use table-driven initialization. --jonas */
	get_opt_int("terminal.linux.type") = 2;
	get_opt_bool("terminal.linux.colors") = 1;
	get_opt_bool("terminal.linux.m11_hack") = 1;
	get_opt_int("terminal.vt100.type") = 1;
	get_opt_int("terminal.vt110.type") = 1;
	get_opt_int("terminal.xterm.type") = 1;
	get_opt_int("terminal.xterm.underline") = 1;
	get_opt_int("terminal.xterm-color.type") = 1;
	get_opt_bool("terminal.xterm-color.colors") = 1;
	get_opt_int("terminal.xterm-color.underline") = 1;
	get_opt_int("terminal.xterm-256color.type") = 1;
	get_opt_bool("terminal.xterm-256color.colors") = 2;
	get_opt_int("terminal.xterm-256color.underline") = 1;

	strcpy(get_opt_str("mime.extension.gif"), image_gif);
	strcpy(get_opt_str("mime.extension.jpg"), image_jpeg);
	strcpy(get_opt_str("mime.extension.jpeg"), image_jpeg);
	strcpy(get_opt_str("mime.extension.png"), image_png);
	strcpy(get_opt_str("mime.extension.txt"), text_plain);
	strcpy(get_opt_str("mime.extension.htm"), text_html);
	strcpy(get_opt_str("mime.extension.html"), text_html);

	strcpy(get_opt_str("protocol.user.mailto.unix"), mailto);
	strcpy(get_opt_str("protocol.user.mailto.unix-xwin"), mailto);
	strcpy(get_opt_str("protocol.user.telnet.unix"), telnet);
	strcpy(get_opt_str("protocol.user.telnet.unix-xwin"), telnet);
	strcpy(get_opt_str("protocol.user.tn3270.unix"), tn3270);
	strcpy(get_opt_str("protocol.user.tn3270.unix-xwin"), tn3270);
	strcpy(get_opt_str("protocol.user.gopher.unix"), gopher);
	strcpy(get_opt_str("protocol.user.gopher.unix-xwin"), gopher);
	strcpy(get_opt_str("protocol.user.news.unix"), news);
	strcpy(get_opt_str("protocol.user.news.unix-xwin"), news);
	strcpy(get_opt_str("protocol.user.irc.unix"), irc);
	strcpy(get_opt_str("protocol.user.irc.unix-xwin"), irc);
}

void
init_options(void)
{
	config_options = add_opt_tree_tree(&options_root, "", "",
					 "config", OPT_LISTBOX | OPT_SORT, "");
	cmdline_options = add_opt_tree_tree(&options_root, "", "",
					    "cmdline", 0, "");
	register_options();
	register_autocreated_options();
	register_change_hooks(change_hooks);
}

static void
free_options_tree(struct list_head *tree)
{
	while (!list_empty(*tree))
		delete_option(tree->next);
}

void
done_options(void)
{
	unregister_options();
	free_options_tree(&options_root_tree);
}

void
register_change_hooks(struct change_hook_info *change_hooks)
{
	int i;

	for (i = 0; change_hooks[i].name; i++) {
		struct option *option = get_opt_rec(config_options,
						    change_hooks[i].name);

		option->change_hook = change_hooks[i].change_hook;
	}
}

void
unmark_options_tree(struct list_head *tree)
{
	struct option *option;

	foreach (option, *tree) {
		option->flags &= ~OPT_WATERMARK;
		if (option->type == OPT_TREE)
			unmark_options_tree(option->value.tree);
	}
}

static int
check_nonempty_tree(struct list_head *options)
{
	struct option *opt;

	foreach (opt, *options) {
		if (opt->type == OPT_TREE) {
			if (check_nonempty_tree(opt->value.tree))
				return 1;
		} else if (!(opt->flags & OPT_WATERMARK)) {
			return 1;
		}
	}

	return 0;
}

void
smart_config_string(struct string *str, int print_comment, int i18n,
		    struct list_head *options, unsigned char *path, int depth,
		    void (*fn)(struct string *, struct option *,
			       unsigned char *, int, int, int, int))
{
	struct option *option;

	foreach (option, *options) {
		int do_print_comment = 1;

		if (option->flags & OPT_HIDDEN ||
		    option->flags & OPT_WATERMARK ||
		    option->type == OPT_ALIAS ||
		    (option->box_item && !option->box_item->visible) /* _template_ */)
			continue;

		/* Is there anything to be printed anyway? */
		if (option->type == OPT_TREE
		    && !check_nonempty_tree(option->value.tree))
			continue;

		/* We won't pop out the description when we're in autocreate
		 * category and not template. It'd be boring flood of
		 * repetitive comments otherwise ;). */

		/* This print_comment parameter is weird. If it is negative, it
		 * means that we shouldn't print comments at all. If it is 1,
		 * we shouldn't print comment UNLESS the option is _template_
		 * or not-an-autocreating-tree (it is set for the first-level
		 * autocreation tree). When it is 2, we can print out comments
		 * normally. */
		/* It is still broken somehow, as it didn't work for terminal.*
		 * (the first autocreated level) by the time I wrote this. Good
		 * summer job for bored mad hackers with spare boolean mental
		 * power. I have better things to think about, personally.
		 * Maybe we should just mark autocreated options somehow ;). */
		if (!print_comment || (print_comment == 1
					&& (strcmp(option->name, "_template_")
					    && (option->flags & OPT_AUTOCREATE
					        && option->type == OPT_TREE))))
			do_print_comment = 0;

		/* Pop out the comment */

		/* For config file, we ignore do_print_comment everywhere
		 * except 1, but sometimes we want to skip the option totally.
		 */
		fn(str, option, path, depth, option->type == OPT_TREE ? print_comment : do_print_comment, 0, i18n);

		fn(str, option, path, depth, do_print_comment, 1, i18n);

		/* And the option itself */

		if (option_types[option->type].write) {
			fn(str, option, path, depth, do_print_comment, 2, i18n);

		} else if (option->type == OPT_TREE) {
			struct string newpath;
			int pc = print_comment;

			if (!init_string(&newpath)) continue; /* OK? */

			if (pc == 2 && option->flags & OPT_AUTOCREATE)
				pc = 1;
			else if (pc == 1 && strcmp(option->name, "_template_"))
				pc = 0;

			fn(str, option, path, depth, /*pc*/1, 3, i18n);

			if (path) {
				add_to_string(&newpath, path);
				add_char_to_string(&newpath, '.');
			}
			add_to_string(&newpath, option->name);
			smart_config_string(str, pc, i18n, option->value.tree,
					    newpath.source, depth + 1, fn);
			done_string(&newpath);

			fn(str, option, path, depth, /*pc*/1, 3, i18n);
		}

		/* TODO: We should maybe clear the touched flag only when really
		 * saving the stuff...? --pasky */
		option->flags &= ~OPT_TOUCHED;
	}
}


/**********************************************************************
 Options handlers
**********************************************************************/

static unsigned char *
eval_cmd(struct option *o, unsigned char ***argv, int *argc)
{
	if (*argc < 1) return gettext("Parameter expected");

	(*argv)++; (*argc)--;	/* Consume next argument */

	parse_config_file(config_options, "-eval", *(*argv - 1), NULL);

	fflush(stdout);

	return NULL;
}

static unsigned char *
forcehtml_cmd(struct option *o, unsigned char ***argv, int *argc)
{
	safe_strncpy(get_opt_str("mime.default_type"), "text/html", MAX_STR_LEN);
	return NULL;
}

static unsigned char *
lookup_cmd(struct option *o, unsigned char ***argv, int *argc)
{
	struct sockaddr_storage *addrs = NULL;
	int addrno, i;

	if (!*argc) return gettext("Parameter expected");
	if (*argc > 1) return gettext("Too many parameters");

	(*argv)++; (*argc)--;
	if (do_real_lookup(*(*argv - 1), &addrs, &addrno, 0)) {
#ifdef HAVE_HERROR
		herror(gettext("error"));
#else
		error(gettext("Host not found"));
#endif
		return "";
	}

	for (i = 0; i < addrno; i++) {
#ifdef IPV6
		struct sockaddr_in6 addr = *((struct sockaddr_in6 *) &(addrs)[i]);
		unsigned char p[INET6_ADDRSTRLEN];

		if (! inet_ntop(addr.sin6_family,
				(addr.sin6_family == AF_INET6 ? (void *) &addr.sin6_addr
							      : (void *) &((struct sockaddr_in *) &addr)->sin_addr),
				p, INET6_ADDRSTRLEN))
			error(gettext("Resolver error"));
		else
			printf("%s\n", p);
#else
		struct sockaddr_in addr = *((struct sockaddr_in *) &(addrs)[i]);
		unsigned char *p = (unsigned char *) &addr.sin_addr.s_addr;

		printf("%d.%d.%d.%d\n", (int) p[0], (int) p[1],
				        (int) p[2], (int) p[3]);
#endif
	}

	if (addrs) mem_free(addrs);

	fflush(stdout);

	return "";
}

static unsigned char *
version_cmd(struct option *o, unsigned char ***argv, int *argc)
{
	printf(gettext("ELinks %s - Text WWW browser\n"), VERSION_STRING);
	fflush(stdout);
	return "";
}


/* Below we handle help usage printing.
 *
 * We're trying to achieve several goals here:
 *
 * - Genericly define a function to print option trees iteratively.
 * - Make output parsable for various doc tools (to make manpages).
 * - Do some non generic fancy stuff like printing semi-aliased
 *   options (like: -?, -h and -help) on one line when printing
 *   short help. */

#define gettext_nonempty(x) (*(x) ? gettext(x) : (x))

static void
print_full_help(struct option *tree, unsigned char *path)
{
	struct option *option;
	unsigned char saved[MAX_STR_LEN];
	unsigned char *savedpos = saved;

	*savedpos = 0;

	foreach (option, *tree->value.tree) {
		enum option_type type = option->type;
		unsigned char *help;
		unsigned char *capt = option->capt;
		unsigned char *desc = (option->desc && *option->desc)
				      ? (unsigned char *) gettext(option->desc)
				      : (unsigned char *) "N/A";

		/* Don't print deprecated aliases and command line options */
		if (type == OPT_ALIAS && tree != cmdline_options)
			continue;

		if (!capt && !strncasecmp(option->name, "_template_", 10))
			capt = (unsigned char *) N_("Template option folder");

		if (!capt) {
			int len = strlen(option->name);
			int max = MAX_STR_LEN - (savedpos - saved);

			safe_strncpy(savedpos, option->name, max);
			safe_strncpy(savedpos + len, ", -", max - len);
			savedpos += len + 3;
			continue;
		}

		help = gettext_nonempty(option_types[option->type].help_str);

		if (type != OPT_TREE)
			printf("    %s%s%s %s ",
				path, saved, option->name, help);

		/* Print the 'title' of each option type. */
		switch (type) {
			case OPT_BOOL:
			case OPT_INT:
			case OPT_LONG:
				printf(gettext("(default: %ld)"),
					(long) option->value.number);
				break;

			case OPT_STRING:
				printf(gettext("(default: \"%s\")"),
					option->value.string);
				break;

			case OPT_ALIAS:
				printf(gettext("(alias for %s)"),
					option->value.string);
				break;

			case OPT_CODEPAGE:
				printf(gettext("(default: %s)"),
					get_cp_name(option->value.number));
				break;

			case OPT_COLOR:
				printf(gettext("(default: #%06x)"),
					option->value.color);
				break;

			case OPT_COMMAND:
				break;

			case OPT_LANGUAGE:
				printf(gettext("(default: \"%s\")"),
					language_to_name(option->value.number));
				break;

			case OPT_TREE:
			{
				int pathlen = strlen(path);
				int namelen = strlen(option->name);

				if (pathlen + namelen + 2 > MAX_STR_LEN)
					continue;

				/* Append option name to path */
				if (pathlen > 0) {
					memcpy(saved, path, pathlen);
					savedpos = saved + pathlen;
				} else {
					savedpos = saved;
				}
				memcpy(savedpos, option->name, namelen + 1);
				savedpos += namelen;

				capt = gettext_nonempty(capt);
				printf("  %s: (%s)", capt, saved);
				break;
			}

			default:
				internal("Invalid option type: %d\n", type);
				break;
		}

		printf("\n    %8s", "");
		{
			int l = strlen(desc);
			int i;

			for (i = 0; i < l; i++) {
				putchar(desc[i]);

				if (desc[i] == '\n')
					printf("    %8s", "");
			}
		}
		printf("\n\n");

		if (option->type == OPT_TREE) {
			memcpy(savedpos, ".", 2);
			print_full_help(option, saved);
		}

		savedpos = saved;
		*savedpos = 0;
	}
}

static void
print_short_help()
{
#define ALIGN_WIDTH 20
	struct option *option;
	struct string string = NULL_STRING;
	struct string *saved = NULL;
	unsigned char align[ALIGN_WIDTH];

	/* Initialize @space used to align captions. */
	memset(align, ' ', sizeof(align) - 1);
	align[sizeof(align) - 1] = 0;

	foreach (option, *cmdline_options->value.tree) {
		unsigned char *capt;
		unsigned char *help;
		unsigned char *info = saved ? saved->source
					    : (unsigned char *) "";
		int len = strlen(option->name);

		/* When no caption is available the option name is 'stacked'
		 * and the caption is shared with next options that has one. */
		if (!option->capt) {
			if (!saved) {
				if (!init_string(&string))
					continue;

				saved = &string;
			}

			add_to_string(saved, option->name);
			add_to_string(saved, ", -");
			continue;
		}

		capt = gettext_nonempty(option->capt);
		help = gettext_nonempty(option_types[option->type].help_str);

		/* When @help string is non empty align at least one space. */
		len = ALIGN_WIDTH - len - strlen(help);
		len -= (saved ? saved->length : 0);
		len = (len < 0) ? !!(*help) : len;

		align[len] = '\0';
		printf("  -%s%s %s%s%s\n", info, option->name, help, align, capt);
		align[len] = ' ';
		if (saved) {
			done_string(saved);
			saved = NULL;
		}
	}
#undef ALIGN_WIDTH
}

#undef gettext_nonempty

static unsigned char *
printhelp_cmd(struct option *option, unsigned char ***argv, int *argc)
{
	version_cmd(NULL, NULL, NULL);
	printf("\n");

	if (!strcmp(option->name, "config-help")) {
		printf(gettext("Configuration options:\n"));
		print_full_help(config_options, "");
	} else {
		printf(gettext("Usage: elinks [OPTION]... [URL]\n\n"));
		printf(gettext("Options:\n"));
		if (!strcmp(option->name, "long-help")) {
			print_full_help(cmdline_options, "-");
		} else {
			print_short_help();
		}
	}

	fflush(stdout);
	return "";
}


static int
change_hook_cache(struct session *ses, struct option *current, struct option *changed)
{
	count_format_cache();
	shrink_memory(0);
	return 0;
}

static int
change_hook_connection(struct session *ses, struct option *current, struct option *changed)
{
	register_bottom_half((void (*)(void *)) check_queue, NULL);
	return 0;
}

static int
change_hook_html(struct session *ses, struct option *current, struct option *changed)
{
	html_interpret(ses);
	draw_formatted(ses);
	load_frames(ses, ses->doc_view);
	process_file_requests(ses);
	print_screen_status(ses);
	return 0;
}

static int
change_hook_terminal(struct session *ses, struct option *current, struct option *changed)
{
	cls_redraw_all_terminals();
	return 0;
}

/* Bit 2 of show means we should always set visibility, otherwise we set it
 * only on templates. */
static void
update_visibility(struct list_head *tree, int show)
{
	struct option *opt;

	foreach (opt, *tree) {
		if (!strcmp(opt->name, "_template_")) {
			if (opt->box_item)
				opt->box_item->visible = (show & 1);

			if (opt->type == OPT_TREE)
				update_visibility(opt->value.tree, show | 2);
		} else {
			if (opt->box_item && (show & 2))
				opt->box_item->visible = (show & 1);

			if (opt->type == OPT_TREE)
				update_visibility(opt->value.tree, show);
		}
	}
}

static int
change_hook_stemplate(struct session *ses, struct option *current, struct option *changed)
{
	update_visibility(config_options->value.tree, changed->value.number);
	return 0;
}

static int
change_hook_language(struct session *ses, struct option *current, struct option *changed)
{
#ifdef ENABLE_NLS
	set_language(changed->value.number);
#endif
	return 0;
}

static struct change_hook_info change_hooks[] = {
	{ "config.show_template",	change_hook_stemplate },
	{ "connection",			change_hook_connection },
	{ "document.browse",		change_hook_html },
	{ "document.cache",		change_hook_cache },
	{ "document.colors",		change_hook_html },
	{ "document.html",		change_hook_html },
	{ "terminal",			change_hook_terminal },
	{ "ui.language",		change_hook_language },
	{ NULL,				NULL },
};

/**********************************************************************
 Options values
**********************************************************************/

#include "config/options.inc"

void
register_option_info(struct option_info info[], struct option *tree)
{
	int i;

	for (i = 0; info[i].path; i++) {
		struct option *option = &info[i].option;
		unsigned char *string;

		if (option->type != OPT_ALIAS && (tree->flags & OPT_LISTBOX)) {
			option->box_item = init_option_listbox_item(option);
			if (!option->box_item) {
				delete_option(option);
				continue;
			}
		}

		switch (option->type) {
			case OPT_TREE:
				option->value.tree = init_options_tree();
				if (!option->value.tree) {
					delete_option(option);
					continue;
				}
				break;
			case OPT_STRING:
				string = mem_alloc(MAX_STR_LEN);
				if (!string) {
					delete_option(option);
					continue;
				}
				safe_strncpy(string, option->value.string, MAX_STR_LEN);
				option->value.string = string;
				break;
			case OPT_COLOR:
				string = option->value.string;
				assert(string);
				decode_color(string, &option->value.color);
				break;
			case OPT_CODEPAGE:
				string = option->value.string;
				assert(string);
				option->value.number = get_cp_index(string);
				break;
			default:
				break;
		}

		add_opt_rec(tree, info[i].path, option);
	}
}

void
unregister_option_info(struct option_info info[], struct option *tree)
{
	int i = 0;

	/* We need to remove the options in inverse order to the order how we
	 * added them. */

	while (info[i].path) i++;

	for (i--; i >= 0; i--)
		delete_option(&info[i].option);
}

static void
register_options(void)
{
	register_option_info(config_options_info, config_options);
	register_option_info(cmdline_options_info, cmdline_options);
}

static void
unregister_options(void)
{
	unregister_option_info(config_options_info, config_options);
	unregister_option_info(cmdline_options_info, cmdline_options);
}
