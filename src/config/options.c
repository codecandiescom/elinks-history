/* Options variables manipulation core */
/* $Id: options.c,v 1.232 2003/06/16 15:22:16 jonas Exp $ */

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
#include "document/html/colors.h"
#include "document/html/parser.h"
#include "document/cache.h"
#include "intl/charsets.h"
#include "intl/gettext/libintl.h"
#include "lowlevel/dns.h"
#include "lowlevel/select.h"
#include "sched/session.h"
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

struct option root_options = {
	NULL_LIST_HEAD,
	"", 0, OPT_TREE, 0, 0,
	NULL, "",
	NULL,
};
struct option cmdline_options = {
	NULL_LIST_HEAD,
	"", 0, OPT_TREE, 0, 0,
	NULL, "",
	NULL,
};

INIT_LIST_HEAD(root_option_box_items);
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

	/* We iteratively call get_opt_rec() each for path_elemets-1, getting
	 * appropriate tree for it and then resolving [path_elemets]. */
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

	foreach (option, *((struct list_head *) tree->ptr)) {
		if (option->name && !strcmp(option->name, name)) {
			mem_free(aname);
			return option;
		}
	}

	if (tree && tree->flags & OPT_AUTOCREATE && !no_autocreate) {
		struct option *template = get_opt_rec(tree, "_template_");

		if (!template) {
			internal("Requested %s should be autocreated but "
				 "%.*s._template_ is missing!",
				 name_, sep - name_, name_);
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
void *
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
	if (!opt->ptr) elinks_internal("Option %s has no value!", name);
#endif

	return opt->ptr;
}

/* Add option to tree. */
static void
add_opt_rec(struct option *tree, unsigned char *path, struct option *option)
{
	struct list_head *cat = tree->ptr;

	if (*path) {
		tree = get_opt_rec(tree, path);
		cat = tree->ptr;
	}
	if (!cat) return;

	if (option->box_item && option->name && !strcmp(option->name, "_template_"))
		option->box_item->visible = get_opt_int("config.show_template");

	if (tree->box_item && option->box_item) {
		option->box_item->depth = tree->box_item->depth + 1;
		option->box_item->root = tree->box_item;

		add_to_list_bottom(tree->box_item->child, option->box_item);
	} else if (option->box_item) {
		add_to_list_bottom(root_option_box_items, option->box_item);
	}

	add_at_pos((struct option *) cat->prev, option);
}

struct option *
add_opt(struct option *tree, unsigned char *path, unsigned char *capt,
	unsigned char *name, enum option_flags flags, enum option_type type,
	int min, int max, void *ptr, unsigned char *desc)
{
	struct option *option = mem_alloc(sizeof(struct option));

	if (!option) return NULL;

	option->name = stracpy(name); /* I hope everyone will like this. */
	option->flags = flags;
	option->type = type;
	option->min = min;
	option->max = max;
	option->ptr = ptr;
	option->capt = capt;
	option->desc = desc;
	option->change_hook = NULL;

	if (option->type == OPT_ALIAS || tree != &root_options) {
		option->box_item = NULL;
	} else {
		option->box_item = mem_calloc(1, sizeof(struct listbox_item));
		if (option->box_item) {
			init_list(option->box_item->child);
			option->box_item->visible = 1;
			option->box_item->translated = 1;
			option->box_item->text = option->capt ? option->capt : option->name;
			option->box_item->box = &option_boxes;
			option->box_item->udata = option;
			option->box_item->type = type == OPT_TREE ? BI_FOLDER : BI_LEAF;
		}
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
free_option_value(struct option *option)
{
	if (option->type == OPT_TREE) {
		free_options_tree((struct list_head *) option->ptr);
	}

	if (option->type == OPT_BOOL ||
			option->type == OPT_INT ||
			option->type == OPT_LONG ||
			option->type == OPT_STRING ||
			option->type == OPT_LANGUAGE ||
			option->type == OPT_CODEPAGE ||
			option->type == OPT_COLOR ||
			option->type == OPT_ALIAS ||
			option->type == OPT_TREE) {
		mem_free(option->ptr);
	}
}

static void
free_option(struct option *option)
{
	free_option_value(option);
	if (option->name) mem_free(option->name);
	if (option->box_item) {
		del_from_list(option->box_item);
		mem_free(option->box_item);
	}
}

void
delete_option(struct option *option)
{
	free_option(option);
	if (option->next) del_from_list(option);
	mem_free(option);
}

struct option *
copy_option(struct option *template)
{
	struct option *option = mem_alloc(sizeof(struct option));

	if (!option) return NULL;

	option->name = template->name ? stracpy(template->name) : NULL;
	option->flags = template->flags;
	option->type = template->type;
	option->min = template->min;
	option->max = template->max;
	option->capt = template->capt;
	option->desc = template->desc;
	option->change_hook = template->change_hook;

	option->box_item = mem_calloc(1, sizeof(struct listbox_item));
	if (option->box_item) {
		init_list(option->box_item->child);
		option->box_item->visible = 1;
		option->box_item->translated = 1;
		option->box_item->text = option->capt ? option->capt : option->name;
		option->box_item->box = &option_boxes;
		option->box_item->udata = option;
		option->box_item->type = template->box_item
						? template->box_item->type
						: template->type == OPT_TREE
							? BI_FOLDER
							: BI_LEAF;
		option->box_item->depth = template->box_item
						? template->box_item->depth
						: 0;
	}

	option->ptr = option_types[template->type].dup
			? option_types[template->type].dup(option, template)
			: template->ptr;

	return option;
}


static void register_options(void);

struct list_head *
init_options_tree(void)
{
	struct list_head *ptr = mem_alloc(sizeof(struct list_head));

	if (ptr) init_list(*ptr);
	return ptr;
}

void
init_options(void)
{
	root_options.ptr = init_options_tree();
	cmdline_options.ptr = init_options_tree();
	register_options();
}

static void
free_options_tree(struct list_head *tree)
{
	struct option *option;

	foreach (option, *tree) {
		free_option(option);
	}

	free_list(*tree);
}

void
done_options(void)
{
	free_options_tree(root_options.ptr);
	mem_free(root_options.ptr);
	free_options_tree(cmdline_options.ptr);
	mem_free(cmdline_options.ptr);
}

void
unmark_options_tree(struct list_head *tree)
{
	struct option *option;

	foreach (option, *tree) {
		option->flags &= ~OPT_WATERMARK;
		if (option->type == OPT_TREE)
			unmark_options_tree((struct list_head *) option->ptr);
	}
}



static int
check_nonempty_tree(struct list_head *options)
{
	struct option *opt;

	foreach (opt, *options) {
		if (opt->type == OPT_TREE) {
			if (check_nonempty_tree((struct list_head *) opt->ptr))
				return 1;
		} else if (!(opt->flags & OPT_WATERMARK)) {
			return 1;
		}
	}

	return 0;
}

void
smart_config_string(unsigned char **str, int *len, int print_comment,
		    struct list_head *options, unsigned char *path, int depth,
		    void (*fn)(unsigned char **, int *, struct option *,
			       unsigned char *, int, int, int))
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
		    && !check_nonempty_tree((struct list_head *) option->ptr))
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
		fn(str, len, option, path, depth, option->type == OPT_TREE ? print_comment : do_print_comment, 0);

		fn(str, len, option, path, depth, do_print_comment, 1);

		/* And the option itself */

		if (option_types[option->type].write) {
			fn(str, len, option, path, depth, do_print_comment, 2);

		} else if (option->type == OPT_TREE) {
			unsigned char *str2 = init_str();
			int len2 = 0;
			int pc = print_comment;

			if (pc == 2 && option->flags & OPT_AUTOCREATE)
				pc = 1;
			else if (pc == 1 && strcmp(option->name, "_template_"))
				pc = 0;

			fn(str, len, option, path, depth, /*pc*/1, 3);

			if (path) {
				add_to_str(&str2, &len2, path);
				add_chr_to_str(&str2, &len2, '.');
			}
			add_to_str(&str2, &len2, option->name);
			smart_config_string(str, len, pc, option->ptr,
					    str2, depth + 1, fn);
			mem_free(str2);

			fn(str, len, option, path, depth, /*pc*/1, 3);
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

	parse_config_file(&root_options, "-eval", *(*argv - 1), NULL, NULL);

	fflush(stdout);

	return NULL;
}

static unsigned char *
forcehtml_cmd(struct option *o, unsigned char ***argv, int *argc)
{
	strcpy(get_opt_str("document.download.default_mime_type"), "text/html");
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


/* -= Welcome to the help usage printing monster method =-
 *
 * We're trying to achieve several goals here:
 * - Genericly define a function to print option trees iteratively.
 * - Do some non generic fancy stuff like printing semi-aliased'
 *   options (like: -?, -h and -help) on one line when doing
 *   caption oriented usage printing.
 *
 * The caption define wether only the captions should be printed.
 * The level means the level of indentation.
 */

#define gettext_nonempty(x) (*(x) ? gettext(x) : (x))

static void
print_full_help(struct option *tree, unsigned char *path)
{
	struct option *option;
	unsigned char saved[MAX_STR_LEN];
	unsigned char *savedpos = saved;

	*savedpos = 0;

	foreach (option, *((struct list_head *) tree->ptr)) {
		enum option_type type = option->type;
		unsigned char *help;
		unsigned char *capt = option->capt;
		unsigned char *desc = (option->desc && *option->desc)
				      ? (unsigned char *) gettext(option->desc)
				      : (unsigned char *) "N/A";

		/* Don't print autocreated options and deprecated aliases */
		if (type == OPT_ALIAS && tree != &cmdline_options)
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

		/* Print the 'title' of each option type. */
		if (type == OPT_INT || type == OPT_BOOL || type == OPT_LONG) {
			printf(gettext("    %s%s%s %s (default: %d)"),
				path, saved, option->name, help,
				*((int *) option->ptr));

		} else if (type == OPT_STRING && option->ptr) {
			printf(gettext("    %s%s%s %s (default: \"%s\")"),
				path, saved, option->name, help,
				(char *) option->ptr);

		} else if (type == OPT_ALIAS) {
			printf(gettext("    %s%s%s %s (alias for %s)"),
				path, saved, option->name, help,
				(char *) option->ptr);

		} else if (type == OPT_CODEPAGE) {
			printf(gettext("    %s%s%s %s (default: %s)"),
				path, saved, option->name, help,
				get_cp_name(* (int *) option->ptr));

		} else if (type == OPT_COLOR) {
			struct rgb *color = (struct rgb *) option->ptr;

			printf(gettext("    %s%s%s %s (default: #%02x%02x%02x)"),
				path, saved, option->name, help,
				color->r, color->g, color->b);

		} else if (type == OPT_COMMAND) {
			printf("    %s%s%s", path, saved, option->name);

		} else if (type == OPT_LANGUAGE) {
			printf(gettext("    %s%s%s %s (default: \"%s\")"),
				path, saved, option->name, help,
				(char *) option->ptr);

		} else if (type == OPT_TREE) {
			int pathlen = strlen(path);
			int namelen = strlen(option->name);

			if (pathlen + namelen + 2 > MAX_STR_LEN) continue;

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
	unsigned char saved[MAX_STR_LEN];
	unsigned char *savedpos = saved;
	unsigned char align[ALIGN_WIDTH];
	int len = 0;

	/* Initialize @space used to align captions. */
	while (len < ALIGN_WIDTH - 1) align[len++] = ' ';
	align[len] = '\0';
	*savedpos = '\0';

	foreach (option, *((struct list_head *) cmdline_options.ptr)) {
		unsigned char *capt;
		unsigned char *help;

		len = strlen(option->name);

		/* When no caption is available the option name is 'stacked'
		 * and the caption is shared with next options that has one. */
		if (!option->capt) {
			int max = MAX_STR_LEN - (savedpos - saved);

			memcpy(savedpos, option->name, len);
			memcpy(savedpos + len, ", -", max - len + 1);
			savedpos += len + 3;
			continue;
		}

		capt = gettext_nonempty(option->capt);
		help = gettext_nonempty(option_types[option->type].help_str);

		/* When @help string is non empty align at least one space. */
		len = ALIGN_WIDTH - len - strlen(help) - (savedpos - saved);
		len = (len < 0) ? (*help) : len;

		align[len] = '\0';
		printf("  -%s%s %s%s%s\n", saved, option->name, help, align, capt);
		align[len] = ' ';
		savedpos = saved;
		*savedpos = 0;
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
		print_full_help(&root_options, "");
	} else {
		printf(gettext("Usage: elinks [OPTION]... [URL]\n\n"));
		printf(gettext("Options:\n"));
		if (!strcmp(option->name, "long-help")) {
			print_full_help(&cmdline_options, "-");
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
	load_frames(ses, ses->screen);
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
			if (opt->box_item) opt->box_item->visible = show & 1;
			if (opt->type == OPT_TREE)
				update_visibility(opt->ptr, show | 2);
		} else {
			if (opt->box_item && show & 2) opt->box_item->visible = show & 1;
			if (opt->type == OPT_TREE)
				update_visibility(opt->ptr, show);
		}
	}
}

static int
change_hook_stemplate(struct session *ses, struct option *current, struct option *changed)
{
	update_visibility(root_options.ptr, *((int *) changed->ptr));
	return 0;
}

static int
change_hook_language(struct session *ses, struct option *current, struct option *changed)
{
/* FIXME */
#ifdef ENABLE_NLS
	set_language(*((int *) changed->ptr));
#endif
	return 0;
}


/**********************************************************************
 Options values
**********************************************************************/

static void
register_options(void)
{
	/* TODO: The change hooks should be added more elegantly! --pasky */

	add_opt_tree("", N_("Bookmarks"),
		"bookmarks", 0,
		N_("Bookmark options."));

	{
	unsigned char *bff =
#ifdef HAVE_LIBEXPAT
		N_("File format for bookmarks (affects both reading and saving):\n"
		"0 is the default ELinks (Links 0.9x compatible) format\n"
		"1 is XBEL universal XML bookmarks format (NO NATIONAL CHARS SUPPORT!)");
#else
		N_("File format for bookmarks (affects both reading and saving):\n"
		"0 is the default ELinks (Links 0.9x compatible) format\n"
		"1 is XBEL universal XML bookmarks format (NO NATIONAL CHARS SUPPORT!)"
		" (DISABLED)");
#endif

	add_opt_int("bookmarks", N_("File format"),
		"file_format", 0, 0, 1, 0,
		bff);
	}


	add_opt_tree("", N_("Configuration system"),
		"config", 0,
		N_("Configuration handling options."));

	add_opt_int("config", N_("Comments"),
		"comments", 0, 0, 3, 3,
		N_("Amount of comments automatically written to the config file:\n"
		"0 is no comments are written\n"
		"1 is only the \"blurb\" (name+type) is written\n"
		"2 is only the description is written\n"
		"3 is full comments are written"));

	add_opt_int("config", N_("Indentation"),
		"indentation", 0, 0, 16, 2,
		N_("Shift width of one indentation level in the configuration\n"
		"file. Zero means that no indentation is performed at all\n"
		"when saving the configuration."));

	add_opt_int("config", N_("Saving style"),
		"saving_style", 0, 0, 3, 3,
		N_("Determines what happens when you tell ELinks to save options:\n"
		"0 is only values of current options are altered\n"
		"1 is values of current options are altered and missing options\n"
		"     are added at the end of the file\n"
		"2 is the configuration file is rewritten from scratch\n"
		"3 is values of current options are altered and missing options\n"
		"     CHANGED during this ELinks session are added at the end of\n"
		"     the file"));

	add_opt_bool("config", N_("Saving style warnings"),
		"saving_style_w", 0, 0,
		N_("This is internal option used when displaying a warning about\n"
		"obsolete config.saving_style. You shouldn't touch it."));

	add_opt_bool("config", N_("Show template"),
		"show_template", 0, 0,
		N_("Show _template_ options in autocreated trees in the options\n"
		"manager and save them to the configuration file."));
	get_opt_rec(&root_options, "config.show_template")->change_hook = change_hook_stemplate;


	add_opt_tree("", N_("Connections"),
		"connection", 0,
		N_("Connection options."));
	get_opt_rec(&root_options, "connection")->change_hook = change_hook_connection;


	add_opt_tree("connection", N_("SSL"),
		"ssl", 0,
		N_("SSL options."));

#ifdef HAVE_OPENSSL
	add_opt_bool("connection.ssl", N_("Verify certificates"),
		"cert_verify", 0, 0,
		N_("Verify the peer's SSL certificate. Note that this\n"
		"needs extensive configuration of OpenSSL by the user."));
#elif defined(HAVE_GNUTLS)
	add_opt_bool("connection.ssl", N_("Verify certificates"),
		"cert_verify", 0, 0,
		N_("Verify the peer's SSL certificate. Note that this\n"
		"probably doesn't work properly at all with GnuTLS."));
#else
	add_opt_bool("connection.ssl", N_("Verify certificates"),
		"cert_verify", 0, 0,
		N_("Verify the peer's SSL certificate."));
#endif


	add_opt_bool("connection", N_("Asynchronous DNS"),
		"async_dns", 0, 1,
		N_("Use asynchronous DNS resolver?"));

	add_opt_int("connection", N_("Maximum connections"),
		"max_connections", 0, 1, 16, 10,
		N_("Maximum number of concurrent connections."));

	add_opt_int("connection", N_("Maximum connections per host"),
		"max_connections_to_host", 0, 1, 8, 2,
		N_("Maximum number of concurrent connections to a given host."));

	add_opt_int("connection", N_("Connection retries"),
		"retries", 0, 0, 16, 3,
		N_("Number of tries to establish a connection.\n"
		   "Zero means try forever."));

	add_opt_int("connection", N_("Receive timeout"),
		"receive_timeout", 0, 1, 1800, 120,
		N_("Receive timeout (in seconds)."));

	add_opt_int("connection", N_("Timeout for non-restartable connections"),
		"unrestartable_receive_timeout", 0, 1, 1800, 600,
		N_("Timeout for non-restartable connections (in seconds)."));



	add_opt_tree("", N_("Cookies"),
		"cookies", 0,
		N_("Cookies options."));

	add_opt_int("cookies", N_("Accept policy"),
		"accept_policy", 0,
		COOKIES_ACCEPT_NONE, COOKIES_ACCEPT_ALL, COOKIES_ACCEPT_ALL,
		N_("Cookies accepting policy:\n"
		"0 is accept no cookies\n"
		"1 is ask for confirmation before accepting cookie (UNIMPLEMENTED)\n"
		"2 is accept all cookies"));

	add_opt_bool("cookies", N_("Paranoid security"),
		"paranoid_security", 0, 0,
		N_("When enabled, we'll require three dots in cookies domain for all\n"
		"non-international domains (instead of just two dots). Please see\n"
		"code (cookies.c:check_domain_security()) for explanation."));

	add_opt_bool("cookies", N_("Saving"),
		"save", 0, 1,
		N_("Load/save cookies from/to disk?"));

	add_opt_bool("cookies", N_("Resaving"),
		"resave", 0, 1,
		N_("Save cookies after each change in cookies list? No effect when\n"
		"cookies_save is off."));



	add_opt_tree("", N_("Document"),
		"document", 0,
		N_("Document options."));

	add_opt_tree("document", N_("Browsing"),
		"browse", 0,
		N_("Document browsing options (mainly interactivity)."));
	get_opt_rec(&root_options, "document.browse")->change_hook = change_hook_html;


	add_opt_tree("document.browse", N_("Accesskeys"),
		"accesskey", 0,
		N_("Options for handling of the accesskey attribute of the active\n"
		"HTML elements."));

	add_opt_bool("document.browse.accesskey", N_("Automatic links following"),
		"auto_follow", 0, 0,
		N_("Automatically follow a link or submit a form if appropriate\n"
		"accesskey is pressed - this is the standard behaviour, but it's\n"
		"considered dangerous."));

	add_opt_int("document.browse.accesskey", N_("Accesskey priority"),
		"priority", 0, 0, 2, 1,
		N_("Priority of 'accesskey' HTML attribute:\n"
		"0 is first try all normal bindings; if it fails, check accesskey\n"
		"1 is first try only frame bindings; if it fails, check accesskey\n"
		"2 is first check accesskey (this can be dangerous)"));


	add_opt_tree("document.browse", N_("Forms"),
		"forms", 0,
		N_("Options for handling of the forms interaction."));

	add_opt_bool("document.browse.forms", N_("Submit form automatically"),
		"auto_submit", 0, 1,
		N_("Automagically submit a form when enter is pressed with a text\n"
		"field selected."));

	add_opt_bool("document.browse.forms", N_("Confirm submission"),
		"confirm_submit", 0, 1,
		N_("Ask for confirmation when submitting a form."));


	add_opt_tree("document.browse", N_("Images"),
		"images", 0,
		N_("Options for handling of images."));

	add_opt_int("document.browse.images", N_("Display style for image links"),
		"file_tags", 0, -1, 500, -1,
		N_("Display [target filename] instead of [IMG] as visible image tags:\n"
		"-1    means always display just [IMG]\n"
		"0     means always display full target filename\n"
		"1-500 means display target filename with this maximal length;\n"
		"      if it is longer, the middle is substituted by an asterisk"));

	add_opt_int("document.browse.images", N_("Image links tagging"),
		"image_link_tagging", 0, 0, 2, 1,
		N_("When to enclose image links:\n"
		"0     means never\n"
		"1     means never if alt or title are provided (old behavior)\n"
		"2     means always"));

	add_opt_string("document.browse.images", N_("Image link prefix"),
		"image_link_prefix", 0, "[",
		N_("Prefix string to use to mark image links."));

	add_opt_string("document.browse.images", N_("Image link suffix"),
		"image_link_suffix", 0, "]",
		N_("Suffix string to use to mark image links."));

	add_opt_bool("document.browse.images", N_("Display image links"),
		"show_as_links", 0, 0,
		N_("Display links to images."));


	add_opt_tree("document.browse", N_("Links"),
		"links", 0,
		N_("Options for handling of links to other documents."));

	add_opt_bool("document.browse.links", N_("Directory highlighting"),
		"color_dirs", 0, 1,
		N_("Highlight links to directories in FTP and local directory listing."));

	add_opt_bool("document.browse.links", N_("Number links"),
		"numbering", 0, 0,
		N_("Display numbers next to the links."));

	add_opt_int("document.browse.links", N_("Number keys select links"),
		"number_keys_select_link", 0, 0, 2, 1,
		N_("Number keys select links rather than specify command prefixes. This\n"
		"is a tristate:\n"
		"0 never\n"
		"1 if document.browse.links.numbering = 1\n"
		"2 always"));

	/* TODO - this is somehow implemented by ff, but disabled
	 * for now as it doesn't work. */
	add_opt_bool("document.browse.links", N_("Wrap-around links cycling"),
		"wraparound", /* 0 */ 0, 0,
		N_("When pressing 'down' on the last link, jump at the first one, and\n"
		"vice versa."));


	add_opt_int("document.browse", N_("Horizontal text margin"),
		"margin_width", 0, 0, 9, 3,
		N_("Horizontal text margin."));

	add_opt_int("document.browse", N_("Scroll margin"),
		"scroll_margin", 0, 0, 20, 3,
		N_("Size of the virtual margin - when you click inside of that margin,\n"
		"document scrolls in that direction."));

	add_opt_int("document.browse", N_("Scroll step"),
		"scroll_step", 0, 0, MAXINT, 2,
		N_("Number of lines to scroll when a key bound to scroll-up or scroll-\n"
		"down is pressed and no prefix was given."));

	add_opt_bool("document.browse", N_("Tables navigation order"),
		"table_move_order", 0, 0,
		N_("Move by columns in table, instead of rows."));



	add_opt_tree("document", N_("Cache"),
		"cache", 0,
		N_("Cache options."));
	get_opt_rec(&root_options, "document.cache")->change_hook = change_hook_cache;

	add_opt_bool("document.cache", N_("Cache informations about redirects"),
		"cache_redirects", 0, 0,
		N_("Cache even redirects sent by server (usually thru HTTP by a 302\n"
		"HTTP code and a Location header). This was the original behaviour\n"
		"for a quite some time, but it causes problems in a situation very\n"
		"common to various web login systems - frequently, when accessing\n"
		"certain location, they will redirect you to a login page if they\n"
		"don't receive an auth cookie, the login page then gives you the\n"
		"cookie and redirects you back to the original page, but there you\n"
		"have already cached redirect back to the login page! If this\n"
		"option has value of 0, this malfunction is fixed, but occassionally\n"
		"you may get superfluous (depends on how you take it ;-) requests to\n"
		"the server. If this option has value of 1, experienced users can\n"
		"still workaround it by clever combination of usage of reload,\n"
		"jumping around in session history and hitting ctrl+enter.\n"
		"Note that this option is checked when retrieving the information\n"
		"from cache, not when saving it to cache - thus if you will enable\n"
		"it, even previous redirects will be taken from cache instead of\n"
		"asking the server."));

	add_opt_bool("document.cache", N_("Ignore cache-control info from server"),
		"ignore_cache_control", 0, 1,
		N_("Ignore Cache-Control and Pragma server headers.\n"
		"When set, the document is cached even with 'Cache-Control: no-cache'."));

	add_opt_tree("document.cache", N_("Formatted documents"),
		"format", 0,
		N_("Format cache options."));

	add_opt_int("document.cache.format", N_("Number"),
		"size", 0, 0, 256, 5,
		N_("Number of cached formatted pages."));

	add_opt_tree("document.cache", N_("Memory cache"),
		"memory", 0,
		N_("Memory cache options."));

	add_opt_int("document.cache.memory", N_("Size"),
		"size", 0, 0, MAXINT, 1048576,
		N_("Memory cache size (in kilobytes)."));



	add_opt_tree("document", N_("Charset"),
		"codepage", 0,
		N_("Charset options."));

	add_opt_codepage("document.codepage", N_("Default codepage"),
		"assume", 0, get_cp_index("ISO-8859-1"),
		N_("Default document codepage."));

	add_opt_bool("document.codepage", N_("Ignore charset info from server"),
		"force_assumed", 0, 0,
		N_("Ignore charset info sent by server."));



	add_opt_tree("document", N_("Default color settings"),
		"colors", 0,
		N_("Default document color settings."));
	get_opt_rec(&root_options, "document.colors")->change_hook = change_hook_html;

	add_opt_color("document.colors", N_("Text color"),
		"text", 0, "#bfbfbf",
		N_("Default text color."));

	add_opt_color("document.colors", N_("Background color"),
		"background", 0, "#000000",
		N_("Default background color."));

	add_opt_color("document.colors", N_("Link color"),
		"link", 0, "#0000ff",
		N_("Default link color."));

	add_opt_color("document.colors", N_("Visited-link color"),
		"vlink", 0, "#ffff00",
		N_("Default visited link color."));

	add_opt_color("document.colors", N_("Directory color"),
		"dirs", 0, "#ffff00",
		N_("Default directory color.\n"
	       	"See document.browse.links.color_dirs option."));

	add_opt_bool("document.colors", N_("Allow dark colors on black background"),
		"allow_dark_on_black", 0, 0,
		N_("Allow dark colors on black background."));

	add_opt_int("document.colors", N_("Use document-specified colors"),
		"use_document_colors", 0, 0, 2, 2,
		N_("Use colors specified in document:\n"
		"0 is use always the default settings\n"
		"1 is use document colors if available, except background\n"
		"2 is use document colors, including background. This can\n"
		"  look really impressive mostly, but few sites look really\n"
		"  ugly there (unfortunately including slashdot (but try to\n"
		"  let him serve you that 'plain' version and the world will\n"
		"  suddenly become a much more happy place for life)). Note\n"
		"  that obviously if the background isn't black, it will\n"
		"  break transparency, if you have it enabled for your terminal\n"
		"  and on your terminal."));



	add_opt_tree("document", N_("Downloading"),
		"download", 0,
		N_("Options regarding files downloading and handling."));

	/* Compatibility alias. Added: 2003-05-07, 0.5pre1.CVS. */
	add_opt_alias("document.download", N_("Default MIME-type"),
		"default_mime_type", 0, "mime.default_type",
		N_("This option is deprecated and will be removed very soon.\n"
		"Please use the mime.default_type option instead."));

	add_opt_string("document.download", N_("Default download directory"),
		"directory", 0, "./",
		N_("Default download directory."));

	add_opt_bool("document.download", N_("Set original time"),
		"set_original_time", 0, 0,
		N_("Set the timestamp of each downloaded file to the timestamp\n"
		"stored on the server."));

	/* Does automatic resuming make sense as an option? */
	add_opt_int("document.download", N_("Prevent overwriting"),
		"overwrite", 0, 0, 2, 2,
		N_("Prevent overwriting the local files:\n"
		"0 is files will silently be overwritten.\n"
		"1 is add a suffix .{number} (for example '.1') to the name.\n"
		"2 is ask the user."));

	add_opt_int("document.download", N_("Notify download completion by bell"),
		"notify_bell", 0, 0, 2, 0,
		N_("Audio notification when download is completed:\n"
		"0 is never.\n"
		"1 is when background notification is active.\n"
		"2 is always"));


	add_opt_tree("document", N_("Dump output"),
		"dump", 0,
		N_("Dump output options."));

	add_opt_codepage("document.dump", N_("Codepage"),
		"codepage", 0, get_cp_index("us-ascii"),
		N_("Codepage used in dump output."));

	add_opt_int("document.dump", N_("Width"),
		"width", 0, 40, 512, 80,
		N_("Width of screen in characters when dumping a HTML document."));



	add_opt_tree("document", N_("History"),
		"history", 0,
		N_("History options."));

	add_opt_tree("document.history", N_("Global history"),
		"global", 0,
		N_("Global history options."));

	/* XXX: Disable global history if -anonymous is given? */
	add_opt_bool("document.history.global", N_("Enable"),
		"enable", 0, 1,
		N_("Enable global history (\"history of all pages visited\")."));

	add_opt_int("document.history.global", N_("Maximum number of entries"),
		"max_items", 0, 1, MAXINT, 1024,
		N_("Maximum number of entries in the global history."));

	add_opt_int("document.history.global", N_("Display style"),
		"display_type", 0, 0, 1, 0,
		N_("What to display in global history dialog:\n"
		"0 is URLs\n"
		"1 is page titles"));

	add_opt_bool("document.history", N_("Keep unhistory"),
		"keep_unhistory", 0, 1,
		N_("Keep unhistory (\"forward history\")?"));



	add_opt_tree("document", N_("HTML rendering"),
		"html", 0,
		N_("Options concerning the display of HTML pages."));
	get_opt_rec(&root_options, "document.html")->change_hook = change_hook_html;

	add_opt_bool("document.html", N_("Display frames"),
		"display_frames", 0, 1,
		N_("Display frames."));

	add_opt_bool("document.html", N_("Display tables"),
		"display_tables", 0, 1,
		N_("Display tables."));

	add_opt_bool("document.html", N_("Display subscripts"),
		"display_subs", 0, 1,
		N_("Display subscripts (as [thing])."));

	add_opt_bool("document.html", N_("Display superscripts"),
		"display_sups", 0, 1,
		N_("Display superscripts (as ^thing)."));



	add_opt_tree("", N_("MIME"),
		"mime", 0,
		N_("MIME-related options (handlers of various MIME types)."));


	add_opt_string("mime", N_("Default MIME-type"),
		"default_type", 0, "application/octet-stream",
		N_("Document MIME-type to assume by default (when we are unable to\n"
		"guess it properly from known information about the document)."));


	add_opt_tree("mime", N_("MIME type associations"),
		"type", OPT_AUTOCREATE,
		N_("Handler <-> MIME type association. The first sub-tree is the MIME\n"
		"class while the second sub-tree is the MIME type (ie. image/gif\n"
		"handler will reside at mime.type.image.gif). Each MIME type option\n"
		"should contain (case-sensitive) name of the MIME handler (its\n"
		"properties are stored at mime.handler.<name>)."));

	add_opt_tree("mime.type", NULL,
		"_template_", OPT_AUTOCREATE,
		N_("Handler matching this MIME-type class ('*' is used here in place\n"
		"of '.')."));

	add_opt_string("mime.type._template_", NULL,
		"_template_", 0, "",
		N_("Handler matching this MIME-type name ('*' is used here in place\n"
		"of '.')."));


	add_opt_tree("mime", N_("File type handlers"),
		"handler", OPT_AUTOCREATE,
		N_("Handler for certain MIME types (as specified in mime.type.*).\n"
		"Each handler usually serves family of MIME types (ie. images)."));

	add_opt_tree("mime.handler", NULL,
		"_template_", OPT_AUTOCREATE,
		N_("Description of this handler."));

	add_opt_tree("mime.handler._template_", NULL,
		"_template_", 0,
		N_("System-specific handler description (ie. unix, unix-xwin, ...)."));

	add_opt_bool("mime.handler._template_._template_", N_("Ask before opening"),
		"ask", 0, 1,
		N_("Ask before opening."));

	add_opt_bool("mime.handler._template_._template_", N_("Block terminal"),
		"block", 0, 1,
		N_("Block the terminal when the handler is running."));

	add_opt_string("mime.handler._template_._template_", N_("Program"),
		"program", 0, "",
		N_("External viewer for this file type. '%' in this string will be\n"
		"substituted by a file name."));


	add_opt_tree("mime", N_("File extension associations"),
		"extension", OPT_AUTOCREATE,
		N_("Extension <-> MIME type association."));

	add_opt_string("mime.extension", NULL,
		"_template_", 0, "",
		N_("MIME-type matching this file extension ('*' is used here in place\n"
		"of '.')."));


	add_opt_tree("mime", N_("Mailcap"),
		"mailcap", 0,
		N_("Options for mailcap support."));

	add_opt_bool("mime.mailcap", N_("Enable"),
		"enable", 0, 1,
		N_("Enable mailcap support."));

	add_opt_string("mime.mailcap", N_("Path"),
		"path", 0, "",
		N_("Mailcap search path. Colon-separated list of files.\n"
		"Leave as \"\" to use MAILCAP environment variable or\n"
		"build-in defaults instead."));

	add_opt_bool("mime.mailcap", N_("Ask before opening"),
		"ask", 0, 1,
		N_("Ask before using the handlers defined by mailcap."));

	add_opt_int("mime.mailcap", N_("Type query string"),
		"description", 0, 0, 2, 0,
		N_("Type of description to show in \"what shall I do with this file\"\n"
		"query dialog:\n"
		"0 is show \"mailcap\".\n"
		"1 is show program to be run.\n"
		"2 is show mailcap description field if any; \"mailcap\" otherwise."));

	add_opt_bool("mime.mailcap", N_("Prioritize entries by file"),
		"prioritize", 0, 1,
		N_("Prioritize entries by the order of the files in the mailcap\n"
		"path. This means that wildcard entries (like: image/*) will\n"
		"also be checked before deciding the handler."));


	add_opt_tree("mime", N_("Mimetypes files"),
		"mimetypes", 0,
		N_("Options for the support of mimetypes files. Mimetypes files\n"
		"can be used to find the content type of an url by looking at the\n"
		"extension of the file name."));

	add_opt_bool("mime.mimetypes", N_("Enable"),
		"enable", 0, 1,
		N_("Enable mime.types support."));

	add_opt_string("mime.mimetypes", N_("Path"),
		"path", 0, "",
		N_("Mimetypes search path. Colon-separated list of files.\n"
		"Leave as \"\" to use build-in default instead."));



	add_opt_tree("", N_("Protocols"),
		"protocol", 0,
		N_("Protocol specific options."));

	add_opt_tree("protocol", N_("HTTP"),
		"http", 0,
		N_("HTTP-specific options."));


	add_opt_tree("protocol.http", N_("Server bug workarounds"),
		"bugs", 0,
		N_("Server-side HTTP bugs workarounds."));

	add_opt_bool("protocol.http.bugs", N_("Do not send Accept-Charset"),
		"accept_charset", 0, 1,
		N_("The Accept-Charset header is quite long and sending it can trigger\n"
		"bugs in some rarely found servers."));

	add_opt_bool("protocol.http.bugs", N_("Allow blacklisting"),
		"allow_blacklist", 0, 1,
		N_("Allow blacklisting of buggy servers."));

	add_opt_bool("protocol.http.bugs", N_("Broken 302 redirects"),
		"broken_302_redirect", 0, 1,
		N_("Broken 302 redirect (violates RFC but compatible with Netscape).\n"
		"This is a problem for a lot of web discussion boards and the like.\n"
		"If they will do strange things to you, try to play with this."));

	add_opt_bool("protocol.http.bugs", N_("No keepalive after POST requests"),
		"post_no_keepalive", 0, 0,
		N_("Disable keepalive connection after POST request."));

	add_opt_bool("protocol.http.bugs", N_("Use HTTP/1.0"),
		"http10", 0, 0,
		N_("Use HTTP/1.0 protocol instead of HTTP/1.1."));

	add_opt_tree("protocol.http", N_("Proxy configuration"),
		"proxy", 0,
		N_("HTTP proxy configuration."));

	add_opt_string("protocol.http.proxy", N_("Host and port-number"),
		"host", 0, "",
		N_("Host and port-number (host:port) of the HTTP proxy, or blank.\n"
		"If it's blank, HTTP_PROXY environment variable is checked as well."));

	add_opt_string("protocol.http.proxy", N_("Username"),
		"user", 0, "",
		N_("Proxy authentication username."));

	add_opt_string("protocol.http.proxy", N_("Password"),
		"passwd", 0, "",
		N_("Proxy authentication password."));


	add_opt_tree("protocol.http", N_("Referer sending"),
		"referer", 0,
		N_("HTTP referer sending options."));

	add_opt_int("protocol.http.referer", N_("Policy"),
		"policy", 0,
		REFERER_NONE, REFERER_TRUE, REFERER_SAME_URL,
		N_("Mode of sending HTTP referer:\n"
		"0 is send no referer\n"
		"1 is send current URL as referer\n"
		"2 is send fixed fake referer\n"
		"3 is send previous URL as referer (correct, but insecure)\n"));

	add_opt_string("protocol.http.referer", N_("Fake referer URL"),
		"fake", 0, "",
		N_("Fake referer to be sent when policy is 2."));


	add_opt_string("protocol.http", N_("Send Accept-Language header"),
		"accept_language", 0, "",
		N_("Send Accept-Language header."));

	add_opt_bool("protocol.http", N_("Use UI language as Accept-Language"),
		"accept_ui_language", 0, 1,
		N_("Request localised versions of documents from web-servers (using the\n"
		"Accept-Language header) using the language you have configured for\n"
		"ELinks' user-interface. Note that some see this as a potential security\n"
		"risk because it tells web-masters about your preference in language."));

	add_opt_bool("protocol.http", N_("Activate HTTP TRACE debugging"),
		"trace", 0, 0,
		N_("If active, all HTTP requests are sent with TRACE as their method\n"
		"rather than GET or POST. This is useful for debugging of both ELinks\n"
		"and various server-side scripts --- the server only returns the client's\n"
		"request back to the client verbatim. Note that this type of request may\n"
		"not be enabled on all servers."));

	add_opt_string("protocol.http", N_("User-agent identification"),
		"user_agent", 0, "ELinks (%v; %s; %t)",
		N_("Change the User Agent ID. That means identification string, which\n"
		"is sent to HTTP server when a document is requested.\n"
		"%v in the string means ELinks version\n"
		"%s in the string means system identification\n"
		"%t in the string means size of the terminal\n"
		"Use \" \" if you don't want any User-Agent header to be sent at all."));



	add_opt_tree("protocol", N_("FTP"),
		"ftp", 0,
		N_("FTP specific options."));

	add_opt_tree("protocol.ftp", N_("Proxy configuration"),
		"proxy", 0,
		N_("FTP proxy configuration."));

	add_opt_string("protocol.ftp.proxy", N_("Host and port-number"),
		"host", 0, "",
		N_("Host and port-number (host:port) of the FTP proxy, or blank.\n"
		"If it's blank, FTP_PROXY environment variable is checked as well."));

	add_opt_string("protocol.ftp", N_("Anonymous password"),
		"anon_passwd", 0, "some@host.domain",
		N_("FTP anonymous password to be sent."));

	add_opt_bool("protocol.ftp", N_("Use passive mode (IPv4)"),
		"use_pasv", 0, 1,
		N_("Use PASV instead of PORT (passive vs active mode, IPv4 only)."));
#ifdef IPV6
	add_opt_bool("protocol.ftp", N_("Use passive mode (IPv6)"),
		"use_epsv", 0, 0,
		N_("Use EPSV instead of EPRT (passive vs active mode, IPv6 only)."));
#else
	add_opt_bool("protocol.ftp", N_("Use passive mode (IPv6)"),
		"use_epsv", 0, 0,
		N_("Use EPSV instead of EPRT (passive vs active mode, IPv6 only).\n"
		"Works only with IPv6 enabled, so nothing interesting for you."));
#endif



	add_opt_tree("protocol", N_("Local files"),
		"file", 0,
		N_("Options specific to local browsing."));

	add_opt_bool("protocol.file", N_("Allow reading special files"),
		"allow_special_files", 0, 0,
		N_("Allow reading from non-regular files? (DANGEROUS - reading\n"
		"/dev/urandom or /dev/zero can ruin your day!)"));

	add_opt_bool("protocol.file", N_("Try encoding extensions"),
		"try_encoding_extensions", 0, 1,
		N_("When set, if we can't open a file named 'filename', we'll try\n"
		"to open 'filename' with some encoding extension appended\n"
		"(ie. 'filename.gz'); it depends on the supported encodings."));


	add_opt_tree("protocol", N_("User protocols"),
		"user", OPT_AUTOCREATE,
		N_("User protocols. Options in this tree specify external\n"
		"handlers for the appropriate protocols. Ie.\n"
		"protocol.user.mailto.unix."));

	/* FIXME: Poorly designed options structure. Ought to be able to specify
	 * need_slashes, free_form and similar options as well :-(. --pasky */

	/* Basically, it looks like protocol.user.mailto.win32 = "blah" */

	add_opt_tree("protocol.user", NULL,
		"_template_", OPT_AUTOCREATE,
		N_("Handler (external program) for this protocol. Name the\n"
		"options in this tree after your system (ie. unix, unix-xwin)."));

	add_opt_string("protocol.user._template_", NULL,
		"_template_", 0, "",
		N_("Handler (external program) for this protocol and system.\n"
		"%h in the string means hostname (or email address)\n"
		"%p in the string means port\n"
		"%d in the string means path (everything after the port)\n"
		"%s in the string means subject (?subject=<this>)\n"
		"%u in the string means the whole URL"));


	/* Start of mailcap compatibility aliases:
	 * Added: 2003-05-07, 0.5pre0.CVS.
	 * Estimated due time: ? */
	add_opt_tree("protocol", N_("Mailcap"),
		"mailcap", 0,
		N_("Options for mailcap support. (Deprecated. Please use\n"
		"mime.mailcap instead)"));

	add_opt_alias("protocol.mailcap", N_("Enable"),
		"enable", 0, "mime.mailcap.enable",
		N_("This option is deprecated and will be removed very soon.\n"
		"Please use the mime.mailcap.enable option instead."));

	add_opt_alias("protocol.mailcap", N_("Path"),
		"path", 0, "mime.mailcap.path",
		N_("This option is deprecated and will be removed very soon.\n"
		"Please use the mime.mailcap.path option instead."));

	add_opt_alias("protocol.mailcap", N_("Ask before opening"),
		"ask", 0, "mime.mailcap.ask",
		N_("This option is deprecated and will be removed very soon.\n"
		"Please use the mime.mailcap.ask option instead."));

	add_opt_int("protocol.mailcap", N_("Type query string"),
		"description", 0, 0, 2, 0,
		N_("This option is deprecated and will be removed very soon.\n"));

	add_opt_alias("protocol.mailcap", N_("Prioritize entries by file"),
		"prioritize", 0, "mime.mailcap.prioritize",
		N_("This option is deprecated and will be removed very soon.\n"
		"Please use the mime.mailcap.prioritize option instead."));


	add_opt_string("protocol", N_("No-proxy domains"),
		"no_proxy", 0, "",
		N_("Comma separated list of domains for which the proxy (HTTP/FTP)\n"
		"should be disabled. Optionally, a port can be specified for some\n"
		"domains as well. If it's blank, NO_PROXY environment variable is\n"
	        "checked as well."));



	add_opt_tree("", N_("Terminals"),
		"terminal", OPT_AUTOCREATE,
		N_("Terminal options."));
	get_opt_rec(&root_options, "terminal")->change_hook = change_hook_terminal;

	add_opt_tree("terminal", NULL,
		"_template_", 0,
		N_("Options specific to this terminal type (according to $TERM value)."));

	add_opt_int("terminal._template_", N_("Border type"),
		"type", 0, 0, 3, 0,
		N_("Terminal type; matters mostly only when drawing frames and\n"
		"dialog box borders:\n"
		"0 is dumb terminal type, ASCII art\n"
		"1 is VT100, simple but portable\n"
		"2 is Linux, you get double frames and other goodies\n"
		"3 is KOI-8"));

	add_opt_bool("terminal._template_", N_("Switch fonts for line drawing"),
		"m11_hack", 0, 0,
		N_("Switch fonts when drawing lines, enabling both local characters\n"
		"and lines working at the same time. Makes sense only with linux\n"
		"terminal."));

	add_opt_bool("terminal._template_", N_("I/O in UTF8"),
		"utf_8_io", 0, 0,
		N_("Enable I/O in UTF8 for Unicode terminals. Note that currently,\n"
		"only the subset of UTF8 according to terminal codepage is used."));

	add_opt_bool("terminal._template_", N_("Restrict CP852"),
		"restrict_852", 0, 0,
		N_("Someone who understands this ... ;)) I'm too lazy to think about this now :P."));

	add_opt_bool("terminal._template_", N_("Block cursor"),
		"block_cursor", 0, 0,
		N_("Move cursor to bottom right corner when done drawing.\n"
		"This is particularly useful when we have a block cursor,\n"
		"so that inversed text is displayed correctly."));

	add_opt_bool("terminal._template_", N_("Use colors"),
		"colors", 0, 0,
		N_("If we should use colors."));

	add_opt_bool("terminal._template_", N_("Enable transparency"),
		"transparency", 0, 1,
		N_("If we should not set the background to black. This is particularly\n"
		"useful when we have a terminal (typically in some windowing\n"
		"environment) with a background image or a transparent background -\n"
		"it will be visible in ELinks as well. Note that this option makes\n"
		"sense only when colors are enabled."));

	add_opt_codepage("terminal._template_", N_("Codepage"),
		"charset", 0, get_cp_index("us-ascii"),
		N_("Codepage of charset used for displaying content on terminal."));



	add_opt_tree("", N_("User interface"),
		"ui", 0,
		N_("User interface options."));



	add_opt_tree("ui", N_("Color settings"),
		"colors", 0,
		N_("Default user interface color settings."));


	/* ========================================================== */
	/* ============= BORING PART (colors) START ================= */
	/* ========================================================== */


	add_opt_tree("ui.colors", N_("Color terminals"),
		"color", 0,
		N_("Color settings for color terminal."));


	add_opt_tree("ui.colors.color", N_("Main menu bar"),
		"mainmenu", 0,
		N_("Main menu bar colors."));

	add_opt_tree("ui.colors.color.mainmenu", N_("Unselected main menu bar item"),
		"normal", 0,
		N_("Unselected main menu bar item colors."));

	add_opt_color("ui.colors.color.mainmenu.normal", N_("Text color"),
		"text", 0, "black",
		N_("Default text color."));

	add_opt_color("ui.colors.color.mainmenu.normal", N_("Background color"),
		"background", 0, "white",
		N_("Default background color."));

	add_opt_tree("ui.colors.color.mainmenu", N_("Selected main menu bar item"),
		"selected", 0,
		N_("Selected main menu bar item colors."));

	add_opt_color("ui.colors.color.mainmenu.selected", N_("Text color"),
		"text", 0, "black",
		N_("Default text color."));

	add_opt_color("ui.colors.color.mainmenu.selected", N_("Background color"),
		"background", 0, "green",
		N_("Default background color."));

	add_opt_tree("ui.colors.color.mainmenu", N_("Hotkey"),
		"hotkey", 0,
		N_("Main menu hotkey colors."));

	add_opt_tree("ui.colors.color.mainmenu.hotkey", N_("Unselected hotkey"),
		"normal", 0,
		N_("Main menu unselected hotkey colors."));

	add_opt_tree("ui.colors.color.mainmenu.hotkey", N_("Selected hotkey"),
		"selected", 0,
		N_("Main menu selected hotkey colors."));

	add_opt_color("ui.colors.color.mainmenu.hotkey.normal", N_("Text color"),
		"text", 0, "darkred",
		N_("Main menu unselected hotkey default text color."));

	add_opt_color("ui.colors.color.mainmenu.hotkey.normal", N_("Background color"),
		"background", 0, "white",
		N_("Main menu unselected hotkey default background color."));

	add_opt_color("ui.colors.color.mainmenu.hotkey.selected", N_("Text color"),
		"text", 0, "darkred",
		N_("Main menu selected hotkey text color."));

	add_opt_color("ui.colors.color.mainmenu.hotkey.selected", N_("Background color"),
		"background", 0, "green",
		N_("Main menu selected hotkey default background color."));


	add_opt_tree("ui.colors.color", N_("Menu bar"),
		"menu", 0,
		N_("Menu bar colors."));

	add_opt_tree("ui.colors.color.menu", N_("Unselected menu item"),
		"normal", 0,
		N_("Unselected menu item colors."));

	add_opt_color("ui.colors.color.menu.normal", N_("Text color"),
		"text", 0, "black",
		N_("Default text color."));

	add_opt_color("ui.colors.color.menu.normal", N_("Background color"),
		"background", 0, "white",
		N_("Default background color."));

	add_opt_tree("ui.colors.color.menu", N_("Selected menu item"),
		"selected", 0,
		N_("Selected menu item colors."));

	add_opt_color("ui.colors.color.menu.selected", N_("Text color"),
		"text", 0, "black",
		N_("Default text color."));

	add_opt_color("ui.colors.color.menu.selected", N_("Background color"),
		"background", 0, "green",
		N_("Default background color."));

	add_opt_tree("ui.colors.color.menu", N_("Hotkey"),
		"hotkey", 0,
		N_("Menu item hotkey colors."));

	add_opt_tree("ui.colors.color.menu.hotkey", N_("Unselected hotkey"),
		"normal", 0,
		N_("Menu item unselected hotkey colors."));

	add_opt_tree("ui.colors.color.menu.hotkey", N_("Selected hotkey"),
		"selected", 0,
		N_("Menu item selected hotkey colors."));

	add_opt_color("ui.colors.color.menu.hotkey.normal", N_("Text color"),
		"text", 0, "darkred",
		N_("Menu item unselected hotkey default text color."));

	add_opt_color("ui.colors.color.menu.hotkey.normal", N_("Background color"),
		"background", 0, "white",
		N_("Menu item unselected hotkey default background color."));

	add_opt_color("ui.colors.color.menu.hotkey.selected", N_("Text color"),
		"text", 0, "darkred",
		N_("Menu item selected hotkey default text color."));

	add_opt_color("ui.colors.color.menu.hotkey.selected", N_("Background color"),
		"background", 0, "green",
		N_("Menu item selected hotkey background color."));

	add_opt_tree("ui.colors.color.menu", N_("Menu frame"),
		"frame", 0,
		N_("Menu frame colors."));

	add_opt_color("ui.colors.color.menu.frame", N_("Text color"),
		"text", 0, "black",
		N_("Default text color."));

	add_opt_color("ui.colors.color.menu.frame", N_("Background color"),
		"background", 0, "white",
		N_("Default background color."));


	add_opt_tree("ui.colors.color", N_("Dialog"),
		"dialog", 0,
		N_("Dialog colors."));

	add_opt_color("ui.colors.color.dialog", N_("Generic background color"),
		"background", 0, "white",
		N_("Dialog generic background color."));

	add_opt_tree("ui.colors.color.dialog", N_("Frame"),
		"frame", 0,
		N_("Dialog frame colors."));

	add_opt_color("ui.colors.color.dialog.frame", N_("Text color"),
		"text", 0, "black",
		N_("Default text color."));

	add_opt_color("ui.colors.color.dialog.frame", N_("Background color"),
		"background", 0, "white",
		N_("Default background color."));

	add_opt_tree("ui.colors.color.dialog", N_("Title"),
		"title", 0,
		N_("Dialog title colors."));

	add_opt_color("ui.colors.color.dialog.title", N_("Text color"),
		"text", 0, "darkred",
		N_("Default text color."));

	add_opt_color("ui.colors.color.dialog.title", N_("Background color"),
		"background", 0, "white",
		N_("Default background color."));

	add_opt_tree("ui.colors.color.dialog", N_("Text"),
		"text", 0,
		N_("Dialog text colors."));

	add_opt_color("ui.colors.color.dialog.text", N_("Text color"),
		"text", 0, "black",
		N_("Default text color."));

	add_opt_color("ui.colors.color.dialog.text", N_("Background color"),
		"background", 0, "white",
		N_("Default background color."));

	add_opt_tree("ui.colors.color.dialog", N_("Checkbox"),
		"checkbox", 0,
		N_("Dialog checkbox colors."));

	add_opt_color("ui.colors.color.dialog.checkbox", N_("Text color"),
		"text", 0, "darkred",
		N_("Default text color."));

	add_opt_color("ui.colors.color.dialog.checkbox", N_("Background color"),
		"background", 0, "white",
		N_("Default background color."));

	add_opt_tree("ui.colors.color.dialog", N_("Checkbox label"),
		"checkbox-label", 0,
		N_("Dialog checkbox label colors."));

	add_opt_color("ui.colors.color.dialog.checkbox-label", N_("Text color"),
		"text", 0, "black",
		N_("Default text color."));

	add_opt_color("ui.colors.color.dialog.checkbox-label", N_("Background color"),
		"background", 0, "white",
		N_("Default background color."));

	add_opt_tree("ui.colors.color.dialog", N_("Button"),
		"button", 0,
		N_("Dialog button colors."));

	add_opt_color("ui.colors.color.dialog.button", N_("Text color"),
		"text", 0, "white",
		N_("Default text color."));

	add_opt_color("ui.colors.color.dialog.button", N_("Background color"),
		"background", 0, "blue",
		N_("Default background color."));

	add_opt_tree("ui.colors.color.dialog", N_("Selected button"),
		"button-selected", 0,
		N_("Dialog selected button colors."));

	add_opt_color("ui.colors.color.dialog.button-selected", N_("Text color"),
		"text", 0, "yellow",
		N_("Default text color."));

	add_opt_color("ui.colors.color.dialog.button-selected", N_("Background color"),
		"background", 0, "green",
		N_("Default background color."));

	add_opt_tree("ui.colors.color.dialog", N_("Text field"),
		"field", 0,
		N_("Dialog text field colors."));

	add_opt_color("ui.colors.color.dialog.field", N_("Text color"),
		"text", 0, "white",
		N_("Default text color."));

	add_opt_color("ui.colors.color.dialog.field", N_("Background color"),
		"background", 0, "blue",
		N_("Default background color."));

	add_opt_tree("ui.colors.color.dialog", N_("Text field text"),
		"field-text", 0,
		N_("Dialog field text colors."));

	add_opt_color("ui.colors.color.dialog.field-text", N_("Text color"),
		"text", 0, "yellow",
		N_("Default text color."));

	add_opt_color("ui.colors.color.dialog.field-text", N_("Background color"),
		"background", 0, "blue",
		N_("Default background color."));

	add_opt_tree("ui.colors.color.dialog", N_("Meter"),
		"meter", 0,
		N_("Dialog meter colors."));

	add_opt_color("ui.colors.color.dialog.meter", N_("Text color"),
		"text", 0, "white",
		N_("Default text color."));

	add_opt_color("ui.colors.color.dialog.meter", N_("Background color"),
		"background", 0, "blue",
		N_("Default background color."));

	add_opt_tree("ui.colors.color.dialog", N_("Shadow"),
		"shadow", 0,
		N_("Dialog shadow colors (see ui.shadows option)."));

	add_opt_color("ui.colors.color.dialog.shadow", N_("Background color"),
		"background", 0, "black",
		N_("Default background color."));


	add_opt_tree("ui.colors.color", N_("Title bar"),
		"title", 0,
		N_("Title bar colors."));

	add_opt_tree("ui.colors.color.title", N_("Generic title bar"),
		"title-bar", 0,
		N_("Generic title bar colors."));

	add_opt_color("ui.colors.color.title.title-bar", N_("Text color"),
		"text", 0, "black",
		N_("Default text color."));

	add_opt_color("ui.colors.color.title.title-bar", N_("Background color"),
		"background", 0, "white",
		N_("Default background color."));

	add_opt_tree("ui.colors.color.title", N_("Title bar text"),
		"title-text", 0,
		N_("Title bar text colors."));

	add_opt_color("ui.colors.color.title.title-text", N_("Text color"),
		"text", 0, "black",
		N_("Default text color."));

	add_opt_color("ui.colors.color.title.title-text", N_("Background color"),
		"background", 0, "white",
		N_("Default background color."));


	add_opt_tree("ui.colors.color", N_("Status bar"),
		"status", 0,
		N_("Status bar colors."));

	add_opt_tree("ui.colors.color.status", N_("Generic status bar"),
		"status-bar", 0,
		N_("Generic status bar colors."));

	add_opt_color("ui.colors.color.status.status-bar", N_("Text color"),
		"text", 0, "black",
		N_("Default text color."));

	add_opt_color("ui.colors.color.status.status-bar", N_("Background color"),
		"background", 0, "white",
		N_("Default background color."));

	add_opt_tree("ui.colors.color.status", N_("Status bar text"),
		"status-text", 0,
		N_("Status bar text colors."));

	add_opt_color("ui.colors.color.status.status-text", N_("Text color"),
		"text", 0, "black",
		N_("Default text color."));

	add_opt_color("ui.colors.color.status.status-text", N_("Background color"),
		"background", 0, "white",
		N_("Default background color."));


	add_opt_tree("ui.colors.color", N_("Tabs bar"),
		"tabs", 0,
		N_("Tabs bar colors."));

	add_opt_tree("ui.colors.color.tabs", N_("Unselected tab"),
		"normal", 0,
		N_("Unselected tab colors."));

	add_opt_color("ui.colors.color.tabs.normal", N_("Text color"),
		"text", 0, "black",
		N_("Default text color."));

	add_opt_color("ui.colors.color.tabs.normal", N_("Background color"),
		"background", 0, "white",
		N_("Default background color."));

	add_opt_tree("ui.colors.color.tabs", N_("Selected tab"),
		"selected", 0,
		N_("Selected tab colors."));

	add_opt_color("ui.colors.color.tabs.selected", N_("Text color"),
		"text", 0, "black",
		N_("Default text color."));

	add_opt_color("ui.colors.color.tabs.selected", N_("Background color"),
		"background", 0, "green",
		N_("Default background color."));

	add_opt_tree("ui.colors.color", N_("Searched strings"),
		"searched", 0,
		N_("Searched string highlight colors."));

	add_opt_color("ui.colors.color.searched", N_("Text color"),
		"text", 0, "black",
		N_("Default text color."));

	add_opt_color("ui.colors.color.searched", N_("Background color"),
		"background", 0, "lime",
		N_("Default background color."));



	add_opt_tree("ui.colors", N_("Non-color terminals"),
		"mono", 0,
		N_("Color settings for non-color terminal."));


	add_opt_tree("ui.colors.mono", N_("Main menu bar"),
		"mainmenu", 0,
		N_("Main menu bar colors."));

	add_opt_tree("ui.colors.mono.mainmenu", N_("Unselected menu bar item"),
		"normal", 0,
		N_("Unselected menu bar item colors."));

	add_opt_color("ui.colors.mono.mainmenu.normal", N_("Text color"),
		"text", 0, "black",
		N_("Default text color."));

	add_opt_color("ui.colors.mono.mainmenu.normal", N_("Background color"),
		"background", 0, "white",
		N_("Default background color."));

	add_opt_tree("ui.colors.mono.mainmenu", N_("Selected menu bar item"),
		"selected", 0,
		N_("Selected menu bar item colors."));

	add_opt_color("ui.colors.mono.mainmenu.selected", N_("Text color"),
		"text", 0, "gray",
		N_("Default text color."));

	add_opt_color("ui.colors.mono.mainmenu.selected", N_("Background color"),
		"background", 0, "black",
		N_("Default background color."));

	add_opt_tree("ui.colors.mono.mainmenu", N_("Hotkey"),
		"hotkey", 0,
		N_("Main menu hotkey colors."));

	add_opt_tree("ui.colors.mono.mainmenu.hotkey", N_("Unselected hotkey"),
		"normal", 0,
		N_("Main menu unselected hotkey colors."));

	add_opt_tree("ui.colors.mono.mainmenu.hotkey", N_("Selected hotkey"),
		"selected", 0,
		N_("Main menu selected hotkey colors."));

	add_opt_color("ui.colors.mono.mainmenu.hotkey.normal", N_("Text color"),
		"text", 0, "black",
		N_("Main menu unselected hotkey default text color."));

	add_opt_color("ui.colors.mono.mainmenu.hotkey.normal", N_("Background color"),
		"background", 0, "white",
		N_("Main menu unselected hotkey default background color."));

	add_opt_color("ui.colors.mono.mainmenu.hotkey.selected", N_("Text color"),
		"text", 0, "black",
		N_("Main menu selected hotkey default text color."));

	add_opt_color("ui.colors.mono.mainmenu.hotkey.selected", N_("Background color"),
		"background", 0, "white",
		N_("Main menu selected hotkey default background color."));


	add_opt_tree("ui.colors.mono", N_("Menu bar"),
		"menu", 0,
		N_("Menu bar colors."));

	add_opt_tree("ui.colors.mono.menu", N_("Unselected menu item"),
		"normal", 0,
		N_("Unselected menu item colors."));

	add_opt_color("ui.colors.mono.menu.normal", N_("Text color"),
		"text", 0, "black",
		N_("Default text color."));

	add_opt_color("ui.colors.mono.menu.normal", N_("Background color"),
		"background", 0, "white",
		N_("Default background color."));

	add_opt_tree("ui.colors.mono.menu", N_("Selected menu item"),
		"selected", 0,
		N_("Selected menu item colors."));

	add_opt_color("ui.colors.mono.menu.selected", N_("Text color"),
		"text", 0, "gray",
		N_("Default text color."));

	add_opt_color("ui.colors.mono.menu.selected", N_("Background color"),
		"background", 0, "black",
		N_("Default background color."));

	add_opt_tree("ui.colors.mono.menu", N_("Hotkey"),
		"hotkey", 0,
		N_("Menu item hotkey colors."));

	add_opt_tree("ui.colors.mono.menu.hotkey", N_("Unselected hotkey"),
		"normal", 0,
		N_("Menu unselected hotkey colors."));

	add_opt_tree("ui.colors.mono.menu.hotkey", N_("Selected hotkey"),
		"selected", 0,
		N_("Menu selected hotkey colors."));

	add_opt_color("ui.colors.mono.menu.hotkey.normal", N_("Text color"),
		"text", 0, "gray",
		N_("Menu unselected hotkey default text color."));

	add_opt_color("ui.colors.mono.menu.hotkey.normal", N_("Background color"),
		"background", 0, "black",
		N_("Menu unselected hotkey default background color."));

	add_opt_color("ui.colors.mono.menu.hotkey.selected", N_("Text color"),
		"text", 0, "gray",
		N_("Menu selected hotkey default text color."));

	add_opt_color("ui.colors.mono.menu.hotkey.selected", N_("Background color"),
		"background", 0, "black",
		N_("Menu selected hotkey default background color."));

	add_opt_tree("ui.colors.mono.menu", N_("Menu frame"),
		"frame", 0,
		N_("Menu frame colors."));

	add_opt_color("ui.colors.mono.menu.frame", N_("Text color"),
		"text", 0, "black",
		N_("Default text color."));

	add_opt_color("ui.colors.mono.menu.frame", N_("Background color"),
		"background", 0, "white",
		N_("Default background color."));


	add_opt_tree("ui.colors.mono", N_("Dialog"),
		"dialog", 0,
		N_("Dialog colors."));

	add_opt_color("ui.colors.mono.dialog", N_("Dialog generic background color"),
		"background", 0, "white",
		N_("Dialog generic background color."));

	add_opt_tree("ui.colors.mono.dialog", N_("Frame"),
		"frame", 0,
		N_("Dialog frame colors."));

	add_opt_color("ui.colors.mono.dialog.frame", N_("Text color"),
		"text", 0, "black",
		N_("Default text color."));

	add_opt_color("ui.colors.mono.dialog.frame", N_("Background color"),
		"background", 0, "white",
		N_("Default background color."));

	add_opt_tree("ui.colors.mono.dialog", N_("Title"),
		"title", 0,
		N_("Dialog title colors."));

	add_opt_color("ui.colors.mono.dialog.title", N_("Text color"),
		"text", 0, "gray",
		N_("Default text color."));

	add_opt_color("ui.colors.mono.dialog.title", N_("Background color"),
		"background", 0, "black",
		N_("Default background color."));

	add_opt_tree("ui.colors.mono.dialog", N_("Text"),
		"text", 0,
		N_("Dialog text colors."));

	add_opt_color("ui.colors.mono.dialog.text", N_("Text color"),
		"text", 0, "black",
		N_("Default text color."));

	add_opt_color("ui.colors.mono.dialog.text", N_("Background color"),
		"background", 0, "white",
		N_("Default background color."));

	add_opt_tree("ui.colors.mono.dialog", N_("Checkbox"),
		"checkbox", 0,
		N_("Dialog checkbox colors."));

	add_opt_color("ui.colors.mono.dialog.checkbox", N_("Text color"),
		"text", 0, "black",
		N_("Default text color."));

	add_opt_color("ui.colors.mono.dialog.checkbox", N_("Background color"),
		"background", 0, "white",
		N_("Default background color."));

	add_opt_tree("ui.colors.mono.dialog", N_("Checkbox label"),
		"checkbox-label", 0,
		N_("Dialog checkbox label colors."));

	add_opt_color("ui.colors.mono.dialog.checkbox-label", N_("Text color"),
		"text", 0, "black",
		N_("Default text color."));

	add_opt_color("ui.colors.mono.dialog.checkbox-label", N_("Background color"),
		"background", 0, "white",
		N_("Default background color."));

	add_opt_tree("ui.colors.mono.dialog", N_("Button"),
		"button", 0,
		N_("Dialog button colors."));

	add_opt_color("ui.colors.mono.dialog.button", N_("Text color"),
		"text", 0, "black",
		N_("Default text color."));

	add_opt_color("ui.colors.mono.dialog.button", N_("Background color"),
		"background", 0, "white",
		N_("Default background color."));

	add_opt_tree("ui.colors.mono.dialog", N_("Selected button"),
		"button-selected", 0,
		N_("Dialog selected button colors."));

	add_opt_color("ui.colors.mono.dialog.button-selected", N_("Text color"),
		"text", 0, "white",
		N_("Default text color."));

	add_opt_color("ui.colors.mono.dialog.button-selected", N_("Background color"),
		"background", 0, "black",
		N_("Default background color."));

	add_opt_tree("ui.colors.mono.dialog", N_("Text field"),
		"field", 0,
		N_("Dialog field colors."));

	add_opt_color("ui.colors.mono.dialog.field", N_("Text color"),
		"text", 0, "gray",
		N_("Default text color."));

	add_opt_color("ui.colors.mono.dialog.field", N_("Background color"),
		"background", 0, "black",
		N_("Default background color."));

	add_opt_tree("ui.colors.mono.dialog", N_("Text field text"),
		"field-text", 0,
		N_("Dialog field text colors."));

	add_opt_color("ui.colors.mono.dialog.field-text", N_("Text color"),
		"text", 0, "gray",
		N_("Default text color."));

	add_opt_color("ui.colors.mono.dialog.field-text", N_("Background color"),
		"background", 0, "black",
		N_("Default background color."));

	add_opt_tree("ui.colors.mono.dialog", N_("Meter"),
		"meter", 0,
		N_("Dialog meter colors."));

	add_opt_color("ui.colors.mono.dialog.meter", N_("Text color"),
		"text", 0, "gray",
		N_("Default text color."));

	add_opt_color("ui.colors.mono.dialog.meter", N_("Background color"),
		"background", 0, "black",
		N_("Default background color."));

	add_opt_tree("ui.colors.mono.dialog", N_("Shadow"),
		"shadow", 0,
		N_("Dialog shadow colors (see ui.shadows option)."));

	add_opt_color("ui.colors.mono.dialog.shadow", N_("Background color"),
		"background", 0, "black",
		N_("Default background color."));


	add_opt_tree("ui.colors.mono", N_("Title bar"),
		"title", 0,
		N_("Title bar colors."));

	add_opt_tree("ui.colors.mono.title", N_("Generic title bar"),
		"title-bar", 0,
		N_("Generic title bar colors."));

	add_opt_color("ui.colors.mono.title.title-bar", N_("Text color"),
		"text", 0, "gray",
		N_("Default text color."));

	add_opt_color("ui.colors.mono.title.title-bar", N_("Background color"),
		"background", 0, "black",
		N_("Default background color."));

	add_opt_tree("ui.colors.mono.title", N_("Title text"),
		"title-text", 0,
		N_("Title bar text colors."));

	add_opt_color("ui.colors.mono.title.title-text", N_("Text color"),
		"text", 0, "gray",
		N_("Default text color."));

	add_opt_color("ui.colors.mono.title.title-text", N_("Background color"),
		"background", 0, "black",
		N_("Default background color."));


	add_opt_tree("ui.colors.mono", N_("Status bar"),
		"status", 0,
		N_("Status bar colors."));

	add_opt_tree("ui.colors.mono.status", N_("Generic status bar"),
		"status-bar", 0,
		N_("Generic status bar colors."));

	add_opt_color("ui.colors.mono.status.status-bar", N_("Text color"),
		"text", 0, "gray",
		N_("Default text color."));

	add_opt_color("ui.colors.mono.status.status-bar", N_("Background color"),
		"background", 0, "black",
		N_("Default background color."));

	add_opt_tree("ui.colors.mono.status", N_("Status bar text"),
		"status-text", 0,
		N_("Status bar text colors."));

	add_opt_color("ui.colors.mono.status.status-text", N_("Text color"),
		"text", 0, "black",
		N_("Default text color."));

	add_opt_color("ui.colors.mono.status.status-text", N_("Background color"),
		"background", 0, "white",
		N_("Default background color."));


	add_opt_tree("ui.colors.mono", N_("Tabs bar"),
		"tabs", 0,
		N_("Tabs bar colors."));

	add_opt_tree("ui.colors.mono.tabs", N_("Unselected tab"),
		"normal", 0,
		N_("Unselected tab colors."));

	add_opt_color("ui.colors.mono.tabs.normal", N_("Text color"),
		"text", 0, "black",
		N_("Default text color."));

	add_opt_color("ui.colors.mono.tabs.normal", N_("Background color"),
		"background", 0, "white",
		N_("Default background color."));

	add_opt_tree("ui.colors.mono.tabs", N_("Selected tab"),
		"selected", 0,
		N_("Selected tab colors."));

	add_opt_color("ui.colors.mono.tabs.selected", N_("Text color"),
		"text", 0, "gray",
		N_("Default text color."));

	add_opt_color("ui.colors.mono.tabs.selected", N_("Background color"),
		"background", 0, "black",
		N_("Default background color."));

	add_opt_tree("ui.colors.mono", N_("Searched strings"),
		"searched", 0,
		N_("Searched string highlight colors."));

	add_opt_color("ui.colors.mono.searched", N_("Text color"),
		"text", 0, "black",
		N_("Default text color."));

	add_opt_color("ui.colors.mono.searched", N_("Background color"),
		"background", 0, "white",
		N_("Default background color."));


	/* ========================================================== */
	/* ============= BORING PART (colors) END =================== */
	/* ========================================================== */


	add_opt_tree("ui", N_("Dialog settings"),
		"dialogs", 0,
		N_("Dialogs-specific appearance and behaviour settings."));

	add_opt_int("ui.dialogs", N_("Minimal height of listbox widget"),
		"listbox_min_height", 0, 1, 20, 10,
		N_("Minimal height of the listbox widget (used e.g. for bookmarks\n"
		"or global history)."));

	add_opt_bool("ui.dialogs", N_("Drop shadows"),
		"shadows", 0, 0,
		N_("Make dialogs drop shadows (the shadows are solid, you can\n"
		"adjust their color by ui.colors.*.dialog.shadow). You may\n"
		"also want to eliminate the wide borders by adjusting setup.h."));


	add_opt_tree("ui", N_("Timer options"),
		"timer", 0,
		N_("Timed action after certain interval of user inactivity. Someone can\n"
		"even find this useful, although you may not believe that."));

#ifdef USE_LEDS
	add_opt_int("ui.timer", N_("Enable"),
		"enable", 0, 0, 2, 0,
		N_("Whether to enable the timer or not:\n"
		"0 is don't count down anything\n"
		"1 is count down, but don't show the timer\n"
		"2 is count down and show the timer near LEDs"));
#else
	add_opt_int("ui.timer", N_("Enable"),
		"enable", 0, 0, 2, 0,
		N_("Whether to enable the timer or not:\n"
		"0 is don't count down anything\n"
		"1 is count down, but don't show the timer\n"
		"2 is count down and show the timer near LEDs (note that this feature is DISABLED)"));
#endif

	add_opt_int("ui.timer", N_("Duration"),
		"duration", 0, 1, 86400, 86400,
		N_("Inactivity timeout. One day should be enough for just everyone (TM)."));

	add_opt_string("ui.timer", N_("Action"),
		"action", 0, "",
		N_("Key-binding action to be triggered when timer reaches zero."));


	add_opt_tree("ui", N_("Window tabs"),
		"tabs", 0,
		N_("Window tabs settings."));

	add_opt_int("ui.tabs", N_("Display tabs bar"),
		"show_bar",  0, 0, 2, 1,
		N_("Show tabs bar on the screen:\n"
		   "0 means never.\n"
		   "1 means only if two or more tabs.\n"
		   "2 means always."));

	add_opt_bool("ui.tabs", N_("Wrap-around tabs cycling"),
		"wraparound", 0, 1,
		N_("When moving right from the last tab, jump at the first one, and\n"
		"vice versa."));



	add_opt_ptr("ui", N_("Language"),
		"language", 0, OPT_LANGUAGE, mem_calloc(1, sizeof(int)),
		N_("Language of user interface. System means that the language will\n"
		"be extracted from the environment dynamically."));
	get_opt_rec(&root_options, "ui.language")->change_hook = change_hook_language;

	/* Compatibility alias: added by pasky at 2002-12-01, 0.4pre20.CVS.
	 * Estimated due time: 2003-02-01 */
	add_opt_alias("ui", NULL,
		"shadows", 0, "ui.dialogs.shadows",
		N_("This option is deprecated and will be removed very soon.\n"
		"Please use the ui.dialog.shadows option instead."));

	add_opt_bool("ui", N_("Display status bar"),
		"show_status_bar", 0, 1,
		N_("Show status bar on the screen."));

	add_opt_bool("ui", N_("Display title bar"),
		"show_title_bar", 0, 1,
		N_("Show title bar on the screen."));

	add_opt_bool("ui", N_("Display goto dialog on startup"),
		"startup_goto_dialog", 0, 0,
		N_("Pop up goto dialog on startup when there's no homepage set."));


	add_opt_bool("ui", N_("Set window title"),
		"window_title", 0, 1,
		N_("Set the window title when running in a windowing environment \n"
		"in an xterm-like terminal. This way the document's title is \n"
		"shown on the window titlebar."));



	/* Compatibility alias: added by pasky at 2002-12-10, 0.4pre24.CVS.
	 * Estimated due time: 2003-02-10 */
	add_opt_alias("", NULL,
		"config_saving_style", 0, "config.saving_style",
		N_("This option is deprecated and will be removed very soon.\n"
		"Please use the config.saving_style option instead."));

	add_opt_bool("", N_("Use secure file saving"),
		"secure_file_saving", 0, 1,
		N_("First write data to 'file.tmp', then rename to 'file' upon\n"
		"successfully finishing this. Note that this relates only to\n"
		"config files, not downloaded files. You may want to disable\n"
		"it, if you want some config file with some exotic permissions.\n"
		"Secure file saving is automagically disabled if file is symlink."));



	/* Commandline options */

	add_opt_bool_tree(&cmdline_options, "", N_("Restrict to anonymous mode"),
		"anonymous", 0, 0,
		N_("Restrict ELinks so that it can run on an anonymous account.\n"
		"No local file browsing, no downloads. Execution of viewers\n"
		"is allowed, but user can't add or modify entries in\n"
		"association table."));

	add_opt_bool_tree(&cmdline_options, "", N_("Autosubmit first form"),
		"auto-submit", 0, 0,
		N_("Go and submit the first form you'll stumble upon."));

	add_opt_int_tree(&cmdline_options, "", N_("Clone session with given ID"),
		"base-session", 0, 0, MAXINT, 0,
		N_("ID of session (ELinks instance) which we want to clone.\n"
		"This is internal ELinks option, you don't want to use it."));

	add_opt_alias_tree(&cmdline_options, "", N_("MIME type to assume for documents"),
		"default-mime-type", 0, "mime.default_type",
		N_("Default MIME type to assume for documents of unknown type."));

	add_opt_bool_tree(&cmdline_options, "", N_("Write formatted version of given URL to stdout"),
		"dump", 0, 0,
		N_("Write a plain-text version of the given HTML document to\n"
		"stdout."));

	add_opt_alias_tree(&cmdline_options, "", N_("Codepage to use with -dump"),
		"dump-charset", 0, "document.dump.codepage",
		N_("Codepage used in dump output."));

	add_opt_alias_tree(&cmdline_options, "", N_("Width of document formatted with -dump"),
		"dump-width", 0, "document.dump.width",
		N_("Width of the dump output."));

	add_opt_command_tree(&cmdline_options, "", N_("Evaluate given configuration option"),
		"eval", 0, eval_cmd,
		N_("Specify elinks.conf config options on the command-line:\n"
		"  -eval 'set protocol.file.allow_special_files = 1'"));

	/* lynx compatibility */
	add_opt_command_tree(&cmdline_options, "", N_("Assume the file is HTML"),
		"force-html", 0, forcehtml_cmd,
		N_("This makes ELinks assume that the files it sees are HTML. This is\n"
		"equivalent to -default-mime-type text/html."));

	/* XXX: -?, -h and -help share the same caption and should be kept in
	 * the current order for usage help printing to be ok */
	add_opt_command_tree(&cmdline_options, "", NULL,
		"?", 0, printhelp_cmd,
		NULL);

	add_opt_command_tree(&cmdline_options, "", NULL,
		"h", 0, printhelp_cmd,
		NULL);

	add_opt_command_tree(&cmdline_options, "", N_("Print usage help and exit"),
		"help", 0, printhelp_cmd,
		N_("Print usage help and exit."));

	add_opt_command_tree(&cmdline_options, "", N_("Print detailed usage help and exit"),
		"long-help", 0, printhelp_cmd,
		N_("Print detailed usage help and exit."));

	add_opt_command_tree(&cmdline_options, "", N_("Print help for configuration options"),
		"config-help", 0, printhelp_cmd,
		N_("Print help on configuration options and exit."));

	add_opt_command_tree(&cmdline_options, "", N_("Look up specified host"),
		"lookup", 0, lookup_cmd,
		N_("Look up specified host."));

	add_opt_bool_tree(&cmdline_options, "", N_("Run as separate instance"),
		"no-connect", 0, 0,
		N_("Run ELinks as a separate instance instead of connecting to an\n"
		"existing instance. Note that normally no runtime state files\n"
		"(bookmarks, history and so on) are written to the disk when\n"
		"this option is used. See also -touch-files."));

	add_opt_bool_tree(&cmdline_options, "", N_("Don't use files in ~/.elinks"),
		"no-home", 0, 0,
		N_("Don't attempt to create and/or use home rc directory (~/.elinks)."));

	add_opt_int_tree(&cmdline_options, "", N_("Connect to session ring with given ID"),
		"session-ring", 0, 0, MAXINT, 0,
		N_("ID of session ring this ELinks session should connect to. ELinks\n"
		"works in so-called session rings, whereby all instances of ELinks\n"
		"are interconnected and share state (cache, bookmarks, cookies,\n"
		"and so on). By default, all ELinks instances connect to session\n"
		"ring 0. You can change that behaviour with this switch and form as\n"
		"many session rings as you want. Obviously, if the session-ring with\n"
	        "this number doesn't exist yet, it's created and this ELinks instance\n"
		"will become the master instance (that usually doesn't matter for you\n"
		"as a user much). Note that you usually don't want to use this unless\n"
	        "you're a developer and you want to do some testing - if you want the\n"
		"ELinks instances each running standalone, rather use the -no-connect\n"
		"command-line option. Also note that normally no runtime state files\n"
		"are written to the disk when this option is used. See also\n"
		"-touch-files."));

	add_opt_bool_tree(&cmdline_options, "", N_("Write the source of given URL to stdout"),
		"source", 0, 0,
		N_("Write the given HTML document in source form to stdout."));

	add_opt_bool_tree(&cmdline_options, "", N_("Read document from stdin"),
		"stdin", 0, 0,
		N_("Open stdin as an HTML document - this is fully equivalent to:\n"
		" -eval 'set protocol.file.allow_special_files = 1' file:///dev/stdin\n"
		"Use whichever suits you more ;-). Note that reading document from\n"
		"stdin WORKS ONLY WHEN YOU USE -dump OR -source!! (I would like to\n"
		"know why you would use -source -stdin, though ;-)"));

	add_opt_bool_tree(&cmdline_options, "", N_("Touch files in ~/.elinks when running with -no-connect/-session-ring"),
		"touch-files", 0, 0,
		N_("Set to 1 to have runtime state files (bookmarks, history, ...)\n"
		"changed even when -no-connect or -session-ring is used; has no\n"
		"effect if not used in connection with any of these options."));

	add_opt_command_tree(&cmdline_options, "", N_("Print version information and exit"),
		"version", 0, version_cmd,
		N_("Print ELinks version information and exit."));


	/* Some default pre-autocreated options. Doh. */

	get_opt_int("terminal.linux.type") = 2;
	get_opt_bool("terminal.linux.colors") = 1;
	get_opt_bool("terminal.linux.m11_hack") = 1;
	get_opt_int("terminal.vt100.type") = 1;
	get_opt_int("terminal.vt110.type") = 1;
	get_opt_int("terminal.xterm.type") = 1;
	get_opt_int("terminal.xterm-color.type") = 1;
	get_opt_bool("terminal.xterm-color.colors") = 1;

	strcpy(get_opt_str("mime.extension.gif"), "image/gif");
	strcpy(get_opt_str("mime.extension.jpg"), "image/jpeg");
	strcpy(get_opt_str("mime.extension.jpeg"), "image/jpeg");
	strcpy(get_opt_str("mime.extension.png"), "image/png");
	strcpy(get_opt_str("mime.extension.txt"), "text/plain");
	strcpy(get_opt_str("mime.extension.htm"), "text/html");
	strcpy(get_opt_str("mime.extension.html"), "text/html");

	strcpy(get_opt_str("protocol.user.mailto.unix"), "mutt %h -s \"%s\"");
	strcpy(get_opt_str("protocol.user.mailto.unix-xwin"), "mutt %h -s \"%s\"");
	strcpy(get_opt_str("protocol.user.telnet.unix"), "telnet %h %p");
	strcpy(get_opt_str("protocol.user.telnet.unix-xwin"), "telnet %h %p");
	strcpy(get_opt_str("protocol.user.tn3270.unix"), "tn3270 %h %p");
	strcpy(get_opt_str("protocol.user.tn3270.unix-xwin"), "tn3270 %h %p");
	strcpy(get_opt_str("protocol.user.gopher.unix"), "lynx %u");
	strcpy(get_opt_str("protocol.user.gopher.unix-xwin"), "lynx %u");
	strcpy(get_opt_str("protocol.user.news.unix"), "lynx %u");
	strcpy(get_opt_str("protocol.user.news.unix-xwin"), "lynx %u");
	strcpy(get_opt_str("protocol.user.irc.unix"), "irc %u");
	strcpy(get_opt_str("protocol.user.irc.unix-xwin"), "irc %u");
}
