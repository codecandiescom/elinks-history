/* Options variables manipulation core */
/* $Id: options.c,v 1.454 2004/07/04 13:06:04 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <string.h>

#include "elinks.h"

#include "main.h" /* shrink_memory() */
#include "bfu/listbox.h"
#include "config/conf.h"
#include "config/dialogs.h"
#include "config/options.h"
#include "config/opttypes.h"
#include "cache/cache.h"
#include "dialogs/status.h"
#include "document/options.h"
#include "globhist/globhist.h"
#include "intl/charsets.h"
#include "intl/gettext/libintl.h"
#include "lowlevel/select.h"
#include "sched/session.h"
#include "terminal/color.h"
#include "terminal/screen.h"
#include "terminal/terminal.h"
#include "util/color.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/draw.h"


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

static void add_opt_rec(struct option *, unsigned char *, struct option *);
static void free_options_tree(struct list_head *, int recursive);

#ifdef CONFIG_DEBUG
/* Detect ending '.' (and some others) in options captions.
 * It will emit a message in debug mode only. --Zas */

#define bad_punct(c) (c != ')' && !isquote(c) && ispunct(c))

void
check_caption(unsigned char *caption)
{
	int len;
	unsigned char c;

	if (!caption) return;

	len = strlen(caption);
	if (!len) return;

	c = caption[len - 1];
	if (isspace(c) || bad_punct(c))
		DBG("bad char at end of caption [%s]", caption);

#ifdef ENABLE_NLS
	caption = gettext(caption);
	len = strlen(caption);
	if (!len) return;

	c = caption[len - 1];
	if (isspace(c) || bad_punct(c))
		DBG("bad char at end of i18n caption [%s]", caption);
#endif
}

#undef bad_punct
#else
#define check_caption(caption)
#endif


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
			DBG("ERROR in get_opt_rec() crawl: %s (%d) -> %s",
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
		mem_free_set(&option->name, stracpy(name));

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
#ifdef CONFIG_DEBUG
	 unsigned char *file, int line,
#endif
	 struct option *tree, unsigned char *name)
{
	struct option *opt = get_opt_rec(tree, name);

#ifdef CONFIG_DEBUG
	errfile = file;
	errline = line;
	if (!opt) elinks_internal("Attempted to fetch nonexisting option %s!", name);

	/* Various sanity checks. */
	switch (opt->type) {
	case OPT_TREE:
		if (!opt->value.tree)
			elinks_internal("Option %s has no value!", name);
		break;
	case OPT_ALIAS:
		elinks_internal("Invalid use of alias %s for option %s!",
				name, opt->value.string);
		break;
	case OPT_STRING:
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
	case OPT_CODEPAGE: /* TODO: check these too. */
	case OPT_LANGUAGE:
	case OPT_COLOR:
		break;
	}
#endif

	return &opt->value;
}

static void
add_opt_sort(struct option *tree, struct option *option, int abi)
{
	struct list_head *cat = tree->value.tree;
	struct list_head *bcat = &tree->box_item->child;
	struct option *pos;

	/* The list is empty, just add it there. */
	if (list_empty(*cat)) {
		add_to_list(*cat, option);
		if (abi) add_to_list(*bcat, option->box_item);

	/* This fits as the last list entry, add it there. This
	 * optimizes the most expensive BUT most common case ;-). */
	} else if ((option->type != OPT_TREE
		    || ((struct option *) cat->prev)->type == OPT_TREE)
		   && strcmp(((struct option *) cat->prev)->name,
			     option->name) <= 0) {
append:
		add_to_list_end(*cat, option);
		if (abi) add_to_list_end(*bcat, option->box_item);

	/* At the end of the list is tree and we are ordinary. That's
	 * clear case then. */
	} else if (option->type != OPT_TREE
		   && ((struct option *) cat->prev)->type == OPT_TREE) {
		goto append;

	/* Scan the list linearly. This could be probably optimized ie.
	 * to choose direction based on the first letter or so. */
	} else {
		struct listbox_item *bpos = (struct listbox_item *) bcat;

		foreach (pos, *cat) {
			/* First move the box item to the current position but
			 * only if the position has not been marked as deleted
			 * and actually has a box_item -- else we will end up
			 * 'overflowing' and causing assertion failure. */
			if (!(pos->flags & OPT_DELETED) && pos->box_item) {
				bpos = bpos->next;
				assert(bpos != (struct listbox_item *) bcat);
			}

			if ((option->type != OPT_TREE
			     || pos->type == OPT_TREE)
			    && strcmp(pos->name, option->name) <= 0)
				continue;

			/* Ordinary options always sort behind trees. */
			if (option->type != OPT_TREE
			    && pos->type == OPT_TREE)
				continue;

			/* The (struct option) add_at_pos() can mess up the
			 * order so that we add the box_item to itself, so
			 * better do it first. */

			/* Always ensure that them _template_ options are
			 * before anything else so a lonely autocreated option
			 * (with _template_ options set to invisible) will be
			 * connected with an upper corner (ascii: `-) instead
			 * of a rotated T (ascii: +-) when displaying it. */
			if (option->type == pos->type
			    && *option->name <= '_'
			    && !strcmp(pos->name, "_template_")) {
				if (abi) add_at_pos(bpos, option->box_item);
				add_at_pos(pos, option);
				break;
			}

			if (abi) add_at_pos(bpos->prev, option->box_item);
			add_at_pos(pos->prev, option);
			break;
		}

		assert(pos != (struct option *) cat);
		assert(bpos != (struct listbox_item *) bcat);
	}
}

/* Add option to tree. */
static void
add_opt_rec(struct option *tree, unsigned char *path, struct option *option)
{
	int abi = 0;

	assert(path && option && tree);
	if (*path) tree = get_opt_rec(tree, path);

	assertm(tree, "Missing option tree for '%s'", path);
	if (!tree->value.tree) return;

	object_nolock(option, "option");

	if (option->box_item && option->name && !strcmp(option->name, "_template_"))
		option->box_item->visible = get_opt_int("config.show_template");

	if (tree->flags & OPT_AUTOCREATE && !option->desc) {
		struct option *template = get_opt_rec(tree, "_template_");

		assert(template);
		option->desc = template->desc;
	}

	abi = (tree->box_item && option->box_item);

	if (abi) {
		/* The config_root tree is a just a placeholder for the
		 * box_items, it actually isn't a real box_item by itself;
		 * these ghosts are indicated by the fact that they have
		 * NULL @next. */
		if (tree->box_item->next) {
			option->box_item->depth = tree->box_item->depth + 1;
			option->box_item->root = tree->box_item;
		}
	}

	if (tree->flags & OPT_SORT) {
		add_opt_sort(tree, option, abi);

	} else {
		add_to_list_end(*tree->value.tree, option);
		if (abi) add_to_list_end(tree->box_item->child, option->box_item);
	}

	update_hierbox_browser(&option_browser);
}

static inline struct listbox_item *
init_option_listbox_item(struct option *option)
{
	struct listbox_item *box = mem_calloc(1, sizeof(struct listbox_item));

	if (!box) return NULL;

	init_list(box->child);
	box->visible = 1;
	box->udata = option;
	box->type = (option->type == OPT_TREE) ? BI_FOLDER : BI_LEAF;

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

	check_caption(option->capt);

	if (option->type != OPT_ALIAS
	    && ((tree->flags & OPT_LISTBOX) || (option->flags & OPT_LISTBOX))) {
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
			decode_color(value, strlen((unsigned char *) value),
					&option->value.color);
			break;
		case OPT_COMMAND:
			option->value.command = value;
			break;
		case OPT_LANGUAGE:
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
delete_option_do(struct option *option, int recursive)
{
	if (option->next) del_from_list(option);

	if (recursive == -1) {
		ERROR("Orphaned option %s", option->name);
	}

	switch (option->type) {
		case OPT_STRING:
			mem_free_if(option->value.string);
			break;
		case OPT_TREE:
			if (!option->value.tree) break;
			if (!recursive && !list_empty(*option->value.tree)) {
				if (option->flags & OPT_AUTOCREATE) {
					recursive = 1;
				} else {
					ERROR("Orphaned unregistered "
						"option in subtree %s!",
						option->name);
					recursive = -1;
				}
			}
			free_options_tree(option->value.tree, recursive);
			mem_free(option->value.tree);
			break;
		default:
			break;
	}

	if (option->box_item)
		done_listbox_item(&option_browser, option->box_item);

	if (option->flags & OPT_ALLOC) {
		mem_free_if(option->name);
		mem_free(option);
	} else if (!option->capt) {
		/* We are probably dealing with a built-in autocreated option
		 * that will be attempted to be deleted when shutting down.
		 * Clear it so nothing will be done later. */
		memset(option, 0, sizeof(struct option));
	}
}

void
mark_option_as_deleted(struct option *option)
{
	switch (option->type) {
		case OPT_TREE:
			if (!option->value.tree) break;
			free_options_tree(option->value.tree, 1);
			break;
		default:
			break;
	}

	if (option->box_item) {
		done_listbox_item(&option_browser, option->box_item);
		option->box_item = NULL;
	}
	option->flags |= OPT_TOUCHED | OPT_DELETED;
}

void
delete_option(struct option *option)
{
	delete_option_do(option, 1);
}

struct option *
copy_option(struct option *template)
{
	struct option *option = mem_calloc(1, sizeof(struct option));

	if (!option) return NULL;

	option->name = null_or_stracpy(template->name);
	option->flags = (template->flags | OPT_ALLOC);
	option->type = template->type;
	option->min = template->min;
	option->max = template->max;
	option->capt = template->capt;
	option->desc = template->desc;
	option->change_hook = template->change_hook;

	option->box_item = init_option_listbox_item(option);
	if (option->box_item) {
		if (template->box_item) {
			option->box_item->type = template->box_item->type;
			option->box_item->depth = template->box_item->depth;
		}
	}

	if (option_types[template->type].dup) {
		option_types[template->type].dup(option, template);
	} else {
		option->value = template->value;
	}

	return option;
}

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
	/* TODO: Use table-driven initialization. --jonas */
	get_opt_int("terminal.linux.type") = 2;
	get_opt_bool("terminal.linux.colors") = 1;
	get_opt_bool("terminal.linux.m11_hack") = 1;
	get_opt_int("terminal.vt100.type") = 1;
	get_opt_int("terminal.vt110.type") = 1;
	get_opt_int("terminal.xterm.type") = 1;
	get_opt_int("terminal.xterm.underline") = 1;
	get_opt_int("terminal.xterm-color.type") = 1;
	get_opt_int("terminal.xterm-color.colors") = COLOR_MODE_16;
	get_opt_int("terminal.xterm-color.underline") = 1;
#ifdef CONFIG_256_COLORS
	get_opt_int("terminal.xterm-256color.type") = 1;
	get_opt_int("terminal.xterm-256color.colors") = COLOR_MODE_256;
	get_opt_int("terminal.xterm-256color.underline") = 1;
#endif

	strcpy(get_opt_str("protocol.user.mailto.unix"), DEFAULT_AC_OPT_MAILTO);
	strcpy(get_opt_str("protocol.user.mailto.unix-xwin"),DEFAULT_AC_OPT_MAILTO);
	strcpy(get_opt_str("protocol.user.telnet.unix"), DEFAULT_AC_OPT_TELNET);
	strcpy(get_opt_str("protocol.user.telnet.unix-xwin"), DEFAULT_AC_OPT_TELNET);
	strcpy(get_opt_str("protocol.user.tn3270.unix"), DEFAULT_AC_OPT_TN3270);
	strcpy(get_opt_str("protocol.user.tn3270.unix-xwin"), DEFAULT_AC_OPT_TN3270);
	strcpy(get_opt_str("protocol.user.gopher.unix"), DEFAULT_AC_OPT_GOPHER);
	strcpy(get_opt_str("protocol.user.gopher.unix-xwin"), DEFAULT_AC_OPT_GOPHER);
	strcpy(get_opt_str("protocol.user.news.unix"), DEFAULT_AC_OPT_NEWS);
	strcpy(get_opt_str("protocol.user.news.unix-xwin"), DEFAULT_AC_OPT_NEWS);
	strcpy(get_opt_str("protocol.user.irc.unix"), DEFAULT_AC_OPT_IRC);
	strcpy(get_opt_str("protocol.user.irc.unix-xwin"), DEFAULT_AC_OPT_IRC);
}

static struct option_info config_options_info[];
extern struct option_info cmdline_options_info[];
static struct change_hook_info change_hooks[];

void
init_options(void)
{
	cmdline_options = add_opt_tree_tree(&options_root, "", "",
					    "cmdline", 0, "");
	register_options(cmdline_options_info, cmdline_options);

	config_options = add_opt_tree_tree(&options_root, "", "",
					 "config", OPT_SORT, "");
	config_options->flags |= OPT_LISTBOX;
	config_options->box_item = &option_browser.root;
	register_options(config_options_info, config_options);

	register_autocreated_options();
	register_change_hooks(change_hooks);
}

static void
free_options_tree(struct list_head *tree, int recursive)
{
	while (!list_empty(*tree))
		delete_option_do(tree->next, recursive);
}

void
done_options(void)
{
	unregister_options(config_options_info, config_options);
	unregister_options(cmdline_options_info, cmdline_options);
	config_options->box_item = NULL;
	free_options_tree(&options_root_tree, 0);
}

void
register_change_hooks(struct change_hook_info *change_hooks)
{
	int i;

	for (i = 0; change_hooks[i].name; i++) {
		struct option *option = get_opt_rec(config_options,
						    change_hooks[i].name);

		assert(option);
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

void
watermark_deleted_options(struct list_head *tree)
{
	struct option *option;

	foreach (option, *tree) {
		if (option->flags & OPT_DELETED)
			option->flags |= OPT_WATERMARK;
		else if (option->type == OPT_TREE)
			watermark_deleted_options(option->value.tree);
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
		fn(str, option, path, depth,
		   option->type == OPT_TREE ? print_comment
					    : do_print_comment,
		   0, i18n);

		fn(str, option, path, depth, do_print_comment, 1, i18n);

		/* And the option itself */

		if (option_types[option->type].write) {
			fn(str, option, path, depth,
			   do_print_comment, 2, i18n);

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


static int
change_hook_cache(struct session *ses, struct option *current, struct option *changed)
{
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
	draw_formatted(ses, 1);
	load_frames(ses, ses->doc_view);
	process_file_requests(ses);
	print_screen_status(ses);
	return 0;
}

static int
change_hook_insert_mode(struct session *ses, struct option *current, struct option *changed)
{
	update_status();
	return 0;
}

static int
change_hook_active_link(struct session *ses, struct option *current, struct option *changed)
{
	if (!global_doc_opts) return 0;

	global_doc_opts->active_link_fg = get_opt_color("document.browse.links.active_link.colors.text");
	global_doc_opts->active_link_bg = get_opt_color("document.browse.links.active_link.colors.background");
	global_doc_opts->color_active_link = get_opt_bool("document.browse.links.active_link.enable_color");
	global_doc_opts->invert_active_link = get_opt_bool("document.browse.links.active_link.invert");
	global_doc_opts->underline_active_link = get_opt_bool("document.browse.links.active_link.underline");
	global_doc_opts->bold_active_link = get_opt_bool("document.browse.links.active_link.bold");

	return 0;
}

static int
change_hook_terminal(struct session *ses, struct option *current, struct option *changed)
{
	cls_redraw_all_terminals();
	return 0;
}

static int
change_hook_ui(struct session *ses, struct option *current, struct option *changed)
{
	update_status();
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

void
update_options_visibility(void)
{
	update_visibility(config_options->value.tree,
			  get_opt_rec(config_options,
				      "config.show_template")->value.number);
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
	{ "document.browse.forms.insert_mode",
					change_hook_insert_mode },
	{ "document.browse.links.active_link",
					change_hook_active_link },
	{ "document.cache",		change_hook_cache },
	{ "document.codepage",		change_hook_html },
	{ "document.colors",		change_hook_html },
	{ "document.html",		change_hook_html },
	{ "document.plain",		change_hook_html },
	{ "terminal",			change_hook_terminal },
	{ "ui.language",		change_hook_language },
	{ "ui",				change_hook_ui },
	{ NULL,				NULL },
};

int
commit_option_values(struct option_resolver *resolvers,
		     struct option *root, union option_value *values, int size)
{
	int touched = 0;
	int i;

	assert(resolvers && root && values && size);

	for (i = 0; i < size; i++) {
		unsigned char *name = resolvers[i].name;
		struct option *option = get_opt_rec(root, name);
		int id = resolvers[i].id;

		if (memcmp(&option->value, &values[id], sizeof(union option_value))) {
			option->value = values[id];
			option->flags |= OPT_TOUCHED;
			touched++;
		}
	}

	return touched;
}

void
checkout_option_values(struct option_resolver *resolvers,
		       struct option *root,
		       union option_value *values, int size)
{
	int i;

	for (i = 0; i < size; i++) {
		unsigned char *name = resolvers[i].name;
		struct option *option = get_opt_rec(root, name);
		int id = resolvers[i].id;

		values[id] = option->value;
	}
}

/**********************************************************************
 Options values
**********************************************************************/

#include "config/options.inc"

void
register_options(struct option_info info[], struct option *tree)
{
	int i;

	for (i = 0; info[i].path; i++) {
		struct option *option = &info[i].option;
		unsigned char *string;

		check_caption(option->capt);

		if (option->type != OPT_ALIAS
		    && ((tree->flags & OPT_LISTBOX)
			|| (option->flags & OPT_LISTBOX))) {
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
				decode_color(string, strlen(string),
						&option->value.color);
				break;
			case OPT_CODEPAGE:
				string = option->value.string;
				assert(string);
				option->value.number = get_cp_index(string);
				break;
			case OPT_BOOL:
			case OPT_INT:
			case OPT_LONG:
			case OPT_LANGUAGE:
			case OPT_COMMAND:
			case OPT_ALIAS:
				break;
		}

		add_opt_rec(tree, info[i].path, option);
	}
}

void
unregister_options(struct option_info info[], struct option *tree)
{
	int i = 0;

	/* We need to remove the options in inverse order to the order how we
	 * added them. */

	while (info[i].path) i++;

	for (i--; i >= 0; i--)
		delete_option_do(&info[i].option, 0);
}
