/* Options variables manipulation core */
/* $Id: options.c,v 1.154 2002/12/13 18:08:19 pasky Exp $ */

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
#include "document/session.h"
#include "document/view.h"
#include "intl/charsets.h"
#include "intl/language.h"
#include "lowlevel/dns.h"
#include "lowlevel/select.h"
#include "protocol/mime.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"


/* TODO? In the past, covered by shadow and legends, remembered only by the
 * ELinks Elders now, options were in hashes (it was not for a long time, after
 * we started to use dynamic options lists and before we really started to use
 * hiearchic options). Hashes might be swift and deft, but they had a flaw and
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
	NULL, NULL,
	"", 0, OPT_TREE, 0, 0,
	NULL, "",
	NULL,
};
struct option cmdline_options = {
	NULL, NULL,
	"", 0, OPT_TREE, 0, 0,
	NULL, "",
	NULL,
};

struct list_head root_option_box_items = {
	&root_option_box_items, &root_option_box_items
};

struct list_head option_boxes = { &option_boxes, &option_boxes };

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

		tree = tree;

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
get_opt_(unsigned char *file, int line, struct option *tree,
	 unsigned char *name)
{
	struct option *opt = get_opt_rec(tree, name);

#ifdef DEBUG
	errfile = file;
	errline = line;
	if (!opt) int_error("Attempted to fetch unexistent option %s!", name);
	if (!opt->ptr) int_error("Option %s has no value!", name);
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

		add_at_pos((struct listbox_item *) tree->box_item->child.prev,
				option->box_item);
	} else if (option->box_item) {
		add_at_pos((struct listbox_item *) root_option_box_items.prev,
				option->box_item);
	}

	add_at_pos((struct option *) cat->prev, option);
}

struct option *
add_opt(struct option *tree, unsigned char *path, unsigned char *name,
	enum option_flags flags, enum option_type type,
	int min, int max, void *ptr,
	unsigned char *desc)
{
	struct option *option = mem_alloc(sizeof(struct option));

	if (!option) return NULL;

	option->name = stracpy(name); /* I hope everyone will like this. */
	option->flags = flags;
	option->type = type;
	option->min = min;
	option->max = max;
	option->ptr = ptr;
	option->desc = desc;
	option->change_hook = NULL;

	if (option->type == OPT_ALIAS || tree != &root_options) {
		option->box_item = NULL;
	} else {
		option->box_item = mem_calloc(1, sizeof(struct listbox_item));
		if (option->box_item) {
			init_list(option->box_item->child);
			option->box_item->visible = 1;
			option->box_item->text = option->name;
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
	option->desc = template->desc;

	option->box_item = mem_calloc(1, sizeof(struct listbox_item));
	if (option->box_item) {
		init_list(option->box_item->child);
		option->box_item->visible = 1;
		option->box_item->text = option->name;
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


static void register_options();

struct list_head *
init_options_tree()
{
	struct list_head *ptr = mem_alloc(sizeof(struct list_head));

	if (ptr) init_list(*ptr);
	return ptr;
}

void
init_options()
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
done_options()
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
				add_to_str(&str2, &len2, ".");
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
	if (*argc < 1) return "Parameter expected";

	(*argv)++; (*argc)--;	/* Consume next argument */

	parse_config_file(&root_options, "-eval", *(*argv - 1), NULL, NULL);

	fflush(stdout);

	return NULL;
}

static unsigned char *
lookup_cmd(struct option *o, unsigned char ***argv, int *argc)
{
	struct sockaddr *addrs = NULL;
	int addrno, i;

	if (!*argc) return "Parameter expected";
	if (*argc > 1) return "Too many parameters";

	(*argv)++; (*argc)--;
	if (do_real_lookup(*(*argv - 1), &addrs, &addrno)) {
#ifdef HAVE_HERROR
		herror("error");
#else
		fprintf(stderr, "error: host not found\n");
#endif
		return "";
	}

	for (i = 0; i < addrno; i++) {
#ifdef IPV6
		struct sockaddr_in6 addr = *((struct sockaddr_in6 *) &((struct sockaddr_storage *) addrs)[i]);
		unsigned char p[INET6_ADDRSTRLEN];

		if (! inet_ntop(addr.sin6_family,
				(addr.sin6_family == AF_INET6 ? (void *) &addr.sin6_addr
							      : (void *) &((struct sockaddr_in *) &addr)->sin_addr),
				p, INET6_ADDRSTRLEN))
			printf("Resolver error.");
		else
			printf("%s\n", p);
#else
		struct sockaddr_in addr = *((struct sockaddr_in *) &((struct sockaddr_storage *) addrs)[i]);
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
	printf("ELinks " VERSION_STRING " - Text WWW browser\n");
	fflush(stdout);
	return "";
}

static unsigned char *
printhelp_cmd(struct option *option, unsigned char ***argv, int *argc)
{
	int action;

	if (!strcmp(option->name, "help-config"))
		action = 1;
	else
		action = 0;

	if (!action) {
		version_cmd(NULL, NULL, NULL);
		printf("\n");

		printf("Usage: elinks [OPTION]... [URL]\n\n");
		printf("Options:\n\n");
	}

	foreachback (option, *((struct list_head *) cmdline_options.ptr)) {
		if (1 /*option->flags & OPT_CMDLINE*/) {
			printf("-%s ", option->name);

			printf("%s\n", option_types[option->type].help_str);

			if (option->desc) {
				int l = strlen(option->desc);
				int i;

				printf("%15s", "");

				for (i = 0; i < l; i++) {
					putchar(option->desc[i]);

					if (option->desc[i] == '\n')
						printf("%15s", "");
				}

				printf("\n");

				if (option->type == OPT_INT ||
				    option->type == OPT_BOOL ||
				    option->type == OPT_LONG)
					printf("%15sDefault: %d\n", "", * (int *) option->ptr);
				else if (option->type == OPT_STRING)
					printf("%15sDefault: %s\n", "", option->ptr ? (char *) option->ptr : "");
			}

			printf("\n");
		}
	}

/*printf("Keys:\n\
 	ESC	 display menu\n\
	^C	 quit\n\
	^P, ^N	 scroll up, down\n\
	[, ]	 scroll left, right\n\
	up, down select link\n\
	->	 follow link\n\
	<-	 go back\n\
	g	 go to url\n\
	G	 go to url based on current url\n\
	/	 search\n\
	?	 search back\n\
	n	 find next\n\
	N	 find previous\n\
	=	 document info\n\
	\\	 document source\n\
	d	 download\n\
	q	 quit\n");*/

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


/**********************************************************************
 Options values
**********************************************************************/

static void
register_options()
{
	/* TODO: The change hooks should be added more ellegantly! --pasky */

	add_opt_tree("",
		"bookmarks", 0,
		"Bookmarks options.");

	{
	unsigned char *bff =
		"Format of the file with saved bookmarks (matters for both\n"
		"reading and saving):\n"
		"0 is the default ELinks (Links 0.9x compatible) format\n"
		"1 is XBEL universal XML bookmarks format (NO NATIONAL CHARS SUPPORT!)"
#ifndef HAVE_LIBEXPAT
		" (DISABLED)"
#endif
		;

	add_opt_int("bookmarks",
		"file_format", 0, 0, 1, 0,
		bff);
	}


	add_opt_tree("",
		"config", 0,
		"Configuration handling options.");

	add_opt_int("config",
		"comments", 0, 0, 3, 3,
		"Amount of comments automatically written to the config file:\n"
		"0 is no comments are written\n"
		"1 is only the \"blurb\" (name+type) is written\n"
		"2 is only the description is written\n"
		"3 is full comments are written");

	add_opt_int("config",
		"indentation", 0, 0, 16, 2,
		"Shift width of one indentation level in the configuration\n"
		"file. Zero means that no indentation is performed at all\n"
		"when saving the configuration to a file.");

	add_opt_int("config",
		"saving_style", 0, 0, 3, 3,
		"Determines what happens when you let ELinks to save options:\n"
		"0 is only values of current options are altered\n"
		"1 is values of current options are altered and missing options\n"
		"     are added at the end of the file\n"
		"2 is the configuration file is rewritten from scratch\n"
		"3 is values of current options are altered and missing options\n"
		"     CHANGED during this ELinks session are added at the end of\n"
		"     the file");

	add_opt_bool("config",
		"saving_style_w", 0, 0,
		"This is internal option used when displaying warning about\n"
		"obsolete config.saving_style. You shouldn't touch it.");

	add_opt_bool("config",
		"show_template", 0, 0,
		"Show _template_ options in autocreated trees in the options\n"
		"manager and save them to the configuration file.");
	get_opt_rec(&root_options, "config.show_template")->change_hook = change_hook_stemplate;


	add_opt_tree("",
		"connection", 0,
		"Connection options.");
	get_opt_rec(&root_options, "connection")->change_hook = change_hook_connection;


	add_opt_tree("connection",
		"ssl", 0,
		"SSL options.");

#ifdef HAVE_OPENSSL
	add_opt_bool("connection.ssl",
		"cert_verify", 0, 0,
		"Verify the peer's SSL certificate. Note that this\n"
		"needs extensive configuration of OpenSSL by the user.");
#elif defined(HAVE_GNUTLS)
	add_opt_bool("connection.ssl",
		"cert_verify", 0, 0,
		"Verify the peer's SSL certificate. Note that this\n"
		"probably doesn't work properly at all with GnuTLS.");
#else
	add_opt_bool("connection.ssl",
		"cert_verify", 0, 0,
		"Verify the peer's SSL certificate.");
#endif


	add_opt_bool("connection",
		"async_dns", 0, 1,
		"Use asynchronous DNS resolver?");

	add_opt_int("connection",
		"max_connections", 0, 1, 16, 10,
		"Maximum number of concurrent connections.");

	add_opt_int("connection",
		"max_connections_to_host", 0, 1, 8, 2,
		"Maximum number of concurrent connection to a given host.");

	add_opt_int("connection",
		"retries", 0, 1, 16, 3,
		"Number of tries to establish a connection.");

	add_opt_int("connection",
		"receive_timeout", 0, 1, 1800, 120,
		"Timeout on receive (in seconds).");

	add_opt_int("connection",
		"unrestartable_receive_timeout", 0, 1, 1800, 600,
		"Timeout on non restartable connections (in seconds).");



	add_opt_tree("",
		"cookies", 0,
		"Cookies options.");

	add_opt_int("cookies",
		"accept_policy", 0,
		COOKIES_ACCEPT_NONE, COOKIES_ACCEPT_ALL, COOKIES_ACCEPT_ALL,
		"Mode of accepting cookies:\n"
		"0 is accept no cookies\n"
		"1 is ask for confirmation before accepting cookie (UNIMPLEMENTED)\n"
		"2 is accept all cookies");

	add_opt_bool("cookies",
		"paranoid_security", 0, 0,
		"When enabled, we'll require three dots in cookies domain for all\n"
		"non-international domains (instead of just two dots). Please see\n"
		"code (cookies.c:check_domain_security()) for further description.");

	add_opt_bool("cookies",
		"save", 0, 1,
		"Load/save cookies from/to disk?");

	add_opt_bool("cookies",
		"resave", 0, 1,
		"Save cookies after each change in cookies list? No effect when\n"
		"cookies_save is off.");



	add_opt_tree("",
		"document", 0,
		"Document options.");

	add_opt_tree("document",
		"browse", 0,
		"Document browsing options (mainly interactivity).");
	get_opt_rec(&root_options, "document.browse")->change_hook = change_hook_html;


	add_opt_tree("document.browse",
		"accesskey", 0,
		"Options for handling accesskey attribute.");

	add_opt_bool("document.browse.accesskey",
		"auto_follow", 0, 0,
		"Automatically follow link / submit form if appropriate accesskey\n"
		"is pressed - this is standard behaviour, however dangerous.");

	add_opt_int("document.browse.accesskey",
		"priority", 0, 0, 2, 1,
		"Priority of 'accesskey' HTML attribute:\n"
		"0 is first try all normal bindings and if it fails, check accesskey\n"
		"1 is first try only frame bindings and if it fails, check accesskey\n"
		"2 is first check accesskey (that can be dangerous)");


	add_opt_tree("document.browse",
		"forms", 0,
		"Options for handling forms interaction.");

	add_opt_bool("document.browse.forms",
		"auto_submit", 0, 1,
		"Automagically submit a form when enter pressed on text field.");

	add_opt_bool("document.browse.forms",
		"confirm_submit", 0, 1,
		"Ask for confirmation when submitting a form.");


	add_opt_tree("document.browse",
		"images", 0,
		"Options for handling of images.");

	add_opt_int("document.browse.images",
		"file_tags", 0, -1, 500, -1,
		"Display [target filename] instead of [IMG] as visible image tags:\n"
		"-1 means always display just [IMG]\n"
		"0 means always display full target filename\n"
		"1-500 means display target filename of this maximal length;\n"
		"      if filename is longer, middle part of it\n"
		"      is stripped and substituted by an asterisk");

	add_opt_bool("document.browse.images",
		"show_as_links", 0, 0,
		"Display links to images.");


	add_opt_tree("document.browse",
		"links", 0,
		"Options for handling of links to other documents.");

	add_opt_bool("document.browse.links",
		"color_dirs", 0, 1,
		"Highlight links to directories when listing local disk content.");

	add_opt_bool("document.browse.links",
		"numbering", 0, 0,
		"Display links numbered.");

	add_opt_int("document.browse.links",
		"number_keys_select_link", 0, 0, 2, 1,
		"Number keys select links rather than specify command prefixes. This\n"
		"is a tristate:\n"
		"0 never\n"
		"1 if document.browse.links.numbering = 1\n"
		"2 always");

	/* TODO - this is somehow implemented by ff, but disabled
	 * for now as it doesn't work. */
	add_opt_bool("document.browse.links",
		"wraparound", /* 0 */ 0, 0,
		"When pressing 'down' on the last link, jump at the first one, and\n"
		"vice versa.");


	add_opt_int("document.browse",
		"margin_width", 0, 0, 9, 3,
		"Horizontal text margin.");

	add_opt_int("document.browse",
		"scroll_margin", 0, 0, 20, 3,
		"Size of the virtual margin - when you click inside of that margin,\n"
		"document scrolls in that direction.");

	add_opt_bool("document.browse",
		"table_move_order", 0, 0,
		"Move by columns in table.");



	add_opt_tree("document",
		"cache", 0,
		"Cache options.");
	get_opt_rec(&root_options, "document.cache")->change_hook = change_hook_cache;

	add_opt_bool("document.cache",
		"ignore_cache_control", 0, 1,
		"Ignore Cache-Control and Pragma server headers.\n"
		"When set, the document is cached even with 'Cache-Control: no-cache'.");

	add_opt_tree("document.cache",
		"format", 0,
		"Format cache options.");

	add_opt_int("document.cache.format",
		"size", 0, 0, 256, 5,
		"Number of cached formatted pages.");

	add_opt_tree("document.cache",
		"memory", 0,
		"Memory cache options.");

	add_opt_int("document.cache.memory",
		"size", 0, 0, MAXINT, 1048576,
		"Memory cache size (in kilobytes).");



	add_opt_tree("document",
		"codepage", 0,
		"Charset options.");

	add_opt_codepage("document.codepage",
		"assume", 0, get_cp_index("ISO-8859-1"),
		"Default document codepage.");

	add_opt_bool("document.codepage",
		"force_assumed", 0, 0,
		"Ignore charset info sent by server.");



	add_opt_tree("document",
		"colors", 0,
		"Default document color settings.");
	get_opt_rec(&root_options, "document.colors")->change_hook = change_hook_html;

	add_opt_color("document.colors",
		"text", 0, "#bfbfbf",
		"Default text color.");

	/* FIXME - this produces ugly results now */
	add_opt_color("document.colors",
		"background", 0, "#000000",
		"Default background color.");

	add_opt_color("document.colors",
		"link", 0, "#0000ff",
		"Default link color.");

	add_opt_color("document.colors",
		"vlink", 0, "#ffff00",
		"Default vlink color.");

	add_opt_color("document.colors",
		"dirs", 0, "#ffff00",
		"Default directories color.\n"
	       	"See document.browse.links.color_dirs option.");

	add_opt_bool("document.colors",
		"allow_dark_on_black", 0, 0,
		"Allow dark colors on black background.");

	add_opt_bool("document.colors",
		"use_document_colors", 0, 1,
		"Use colors specified in document.");



	add_opt_tree("document",
		"download", 0,
		"Options regarding files downloading and handling.");

	add_opt_string("document.download",
		"default_mime_type", 0, "application/octet-stream",
		"MIME type for a document we should assume by default (when we are\n"
		"unable to guess it properly from known informations about the\n"
		"document).");

	add_opt_string("document.download",
		"directory", 0, "./",
		"Default download directory.");

	add_opt_bool("document.download",
		"set_original_time", 0, 0,
		"Set time of downloaded files accordingly to one stored on server.");

	add_opt_int("document.download",
		"overwrite", 0, 0, 1, 0,
		"Prevention of overwriting of the local files:\n"
		"0 is files will silently be overwritten.\n"
		"1 is add the suffix .{number} (for example '.1') to the name.");

	add_opt_int("document.download",
		"notify_bell", 0, 0, 2, 0,
		"Audio notification when download is completed:\n"
		"0 is never.\n"
		"1 is when background notification is active.\n"
		"2 is always");

	add_opt_tree("document",
		"dump", 0,
		"Dump output options.");

	add_opt_codepage("document.dump",
		"codepage", 0, get_cp_index("us-ascii"),
		"Codepage used in dump output.");

	add_opt_int("document.dump",
		"width", 0, 40, 512, 80,
		"Size of screen in characters, when dumping a HTML document.");



	add_opt_tree("document",
		"history", 0,
		"History options.");

	add_opt_tree("document.history",
		"global", 0,
		"Global history options.");

	/* XXX: Disable global history if -anonymous is given? */
	add_opt_bool("document.history.global",
		"enable", 0, 1,
		"Enable global history (\"history of all pages visited\")?");

	add_opt_int("document.history.global",
		"max_items", 0, 1, MAXINT, 1024,
		"Maximum number of entries in the global history.");

	add_opt_int("document.history.global",
		"display_type", 0, 0, 1, 0,
		"What to display in global history dialog:\n"
		"0 is URLs\n"
		"1 is page titles");

	add_opt_bool("document.history",
		"keep_unhistory", 0, 1,
		"Keep unhistory (\"forward history\")?");



	add_opt_tree("document",
		"html", 0,
		"Options concerning displaying of HTML pages.");
	get_opt_rec(&root_options, "document.html")->change_hook = change_hook_html;

	add_opt_bool("document.html",
		"display_frames", 0, 1,
		"Display frames.");

	add_opt_bool("document.html",
		"display_tables", 0, 1,
		"Display tables.");

	add_opt_bool("document.html",
		"display_subs", 0, 1,
		"Display subscripts (as [thing]).");

	add_opt_bool("document.html",
		"display_sups", 0, 1,
		"Display superscripts (as ^thing).");




	add_opt_tree("",
		"mime", 0,
		"MIME-related options.");


	/* Basically, it will look like mime.type.image.gif = "Picture" */

	add_opt_tree("mime",
		"type", OPT_AUTOCREATE,
		"Action<->MIME-type association.");

	add_opt_tree("mime.type",
		"_template_", OPT_AUTOCREATE,
		"Handler matching this MIME-type class ('*' is used here in place\n"
		"of '.').");

	add_opt_string("mime.type._template_",
		"_template_", 0, "",
		"Handler matching this MIME-type name ('*' is used here in place\n"
		"of '.').");


	add_opt_tree("mime",
		"handler", OPT_AUTOCREATE,
		"Handler for certain file types.");

	add_opt_tree("mime.handler",
		"_template_", OPT_AUTOCREATE,
		"Handler description for this file type.");

	add_opt_tree("mime.handler._template_",
		"_template_", 0,
		"System-specific handler description.");

	add_opt_bool("mime.handler._template_._template_",
		"ask", 0, 1,
		"Ask before opening.");

	add_opt_bool("mime.handler._template_._template_",
		"block", 0, 1,
		"Block the terminal when the handler is running.");

	add_opt_string("mime.handler._template_._template_",
		"program", 0, "",
		"External viewer for this file type.");


	add_opt_tree("mime",
		"extension", OPT_AUTOCREATE,
		"Extension<->MIME-type association.");

	add_opt_string("mime.extension",
		"_template_", 0, "",
		"MIME-type matching this file extension ('*' is used here in place\n"
		"of '.').");



	add_opt_tree("",
		"protocol", 0,
		"Protocol specific options.");

	add_opt_tree("protocol",
		"http", 0,
		"HTTP specific options.");


	add_opt_tree("protocol.http",
		"bugs", 0,
		"Server-side HTTP bugs workarounds.");

	add_opt_bool("protocol.http.bugs",
		"allow_blacklist", 0, 1,
		"Allow blacklist of buggy servers.");

	add_opt_bool("protocol.http.bugs",
		"broken_302_redirect", 0, 1,
		"Broken 302 redirect (violates RFC but compatible with Netscape).");

	add_opt_bool("protocol.http.bugs",
		"post_no_keepalive", 0, 0,
		"No keepalive connection after POST request.");

	add_opt_bool("protocol.http.bugs",
		"http10", 0, 0,
		"Use HTTP/1.0 protocol instead of HTTP/1.1.");


	add_opt_tree("protocol.http",
		"proxy", 0,
		"HTTP proxy configuration.");

	add_opt_string("protocol.http.proxy",
		"host", 0, "",
		"Host and port number (host:port) of the HTTP proxy, or blank.\n"
		"If it's blank, HTTP_PROXY environment variable is checked as well.");

	add_opt_string("protocol.http.proxy",
		"user", 0, "",
		"Proxy authentication user.");

	add_opt_string("protocol.http.proxy",
		"passwd", 0, "",
		"Proxy authentication password.");


	add_opt_tree("protocol.http",
		"referer", 0,
		"HTTP referer sending rules.");

	add_opt_int("protocol.http.referer",
		"policy", 0,
		REFERER_NONE, REFERER_TRUE, REFERER_SAME_URL,
		"Mode of sending HTTP referer:\n"
		"0 is send no referer\n"
		"1 is send current URL as referer\n"
		"2 is send fixed fake referer\n"
		"3 is send previous URL as referer (correct, but insecure)\n");

	add_opt_string("protocol.http.referer",
		"fake", 0, "",
		"Fake referer to be sent when policy is 2.");


	add_opt_string("protocol.http",
		"accept_language", 0, "",
		"Send Accept-Language header.");

	add_opt_bool("protocol.http",
		"accept_ui_language", 0, 1,
		"Use the language of the interface as Accept-Language.");

	add_opt_string("protocol.http",
		"user_agent", 0, "ELinks (%v; %s; %t)",
		"Change the User Agent ID. That means identification string, which\n"
		"is sent to HTTP server when a document is requested.\n"
		"%v in the string means ELinks version\n"
		"%s in the string means system identification\n"
		"%t in the string means size of the terminal\n"
		"Use \" \" if you don't want any User-Agent header to be sent at all.");



	add_opt_tree("protocol",
		"ftp", 0,
		"FTP specific options.");
	add_opt_bool("protocol.ftp",
		"use_pasv", 0, 1,
		"Use PASV instead of PORT (passive vs active mode, IPv4 only).");
#ifdef IPV6
	add_opt_bool("protocol.ftp",
		"use_epsv", 0, 0,
		"** Not yet implemented properly **\n"
		"Use EPSV instead of EPRT (passive vs active mode, IPv6 only).");
#else
	add_opt_bool("protocol.ftp",
		"use_epsv", 0, 0,
		"Use EPSV instead of EPRT (passive vs active mode, IPv6 only).\n"
		"Works only with IPv6 enabled, so nothing interesting for you.");
#endif
	add_opt_tree("protocol.ftp",
		"proxy", 0,
		"FTP proxy configuration.");

	add_opt_string("protocol.ftp.proxy",
		"host", 0, "",
		"Host and port number (host:port) of the FTP proxy, or blank.\n"
		"If it's blank, FTP_PROXY environment variable is checked as well.");

	add_opt_string("protocol.ftp",
		"anon_passwd", 0, "some@host.domain",
		"FTP anonymous password to be sent.");



	add_opt_tree("protocol",
		"file", 0,
		"Options specific for local browsing.");

	add_opt_bool("protocol.file",
		"allow_special_files", 0, 0,
		"Allow reading from non-regular files? (DANGEROUS - reading\n"
		"/dev/urandom or /dev/zero can ruin your day!)");

	add_opt_bool("protocol.file",
		"try_encoding_extensions", 0, 1,
		"When set, if we can't open a file named 'filename', we'll try\n"
		"to open 'filename' + some encoding extensions.\n"
		"(ie. 'filename.gz', it depends of supported encoding)");


	add_opt_tree("protocol",
		"user", OPT_AUTOCREATE,
		"User protocols options.");

	/* FIXME: Poorly designed options structure. Ought to be able to specify
	 * need_slashes, free_form and similiar options as well :-(. --pasky */

	/* Basically, it looks like protocol.user.mailto.win32 = "blah" */

	add_opt_tree("protocol.user",
		"_template_", OPT_AUTOCREATE,
		"System-specific handlers for these protocols.");

	add_opt_string("protocol.user._template_",
		"_template_", 0, "",
		"Handler (external program) for this protocol.\n"
		"%h in the string means hostname (or email address)\n"
		"%p in the string means port\n"
		"%s in the string means subject (?subject=<this>)\n"
		"%u in the string means the whole URL");


	add_opt_tree("protocol",
		"mailcap", 0,
		"Options for mailcap support.");

	add_opt_bool("protocol.mailcap",
		"enable", 0, 1,
		"Enable mailcap support?");

	add_opt_string("protocol.mailcap",
		"path", 0, "",
		"Mailcap search path. List of files are colon seperated.\n"
		"Leave as \"\" to use MAILCAP environment or build in\n"
		"defaults instead.");

	add_opt_bool("protocol.mailcap",
		"ask", 0, 1,
		"Wether to perform query when using handlers provided from\n"
		"mailcap entries.");

	
	add_opt_string("protocol",
		"no_proxy", 0, "",
		"Comma separated list of domains for which the proxy (HTTP/FTP)\n"
		"should be disabled. Optionally, port can be specified for some\n"
		"domains as well. If it's blank, NO_PROXY environment variable is\n"
	        "checked as well.");



	add_opt_tree("",
		"terminal", OPT_AUTOCREATE,
		"Terminal options.");

	add_opt_tree("terminal",
		"_template_", 0,
		"Options specific to this terminal type.");

	add_opt_int("terminal._template_",
		"type", 0, 0, 3, 0,
		"Terminal type; matters mostly only when drawing frames and\n"
		"dialog box borders:\n"
		"0 is dumb terminal type, ASCII art\n"
		"1 is VT100, simple but portable\n"
		"2 is Linux, you get double frames and other goodies\n"
		"3 is KOI-8");

	add_opt_bool("terminal._template_",
		"m11_hack", 0, 0,
		"Switch font when drawing lines, enabling both local characters\n"
		"and lines working at the same time. Makes sense only with linux\n"
		"terminal.");

	add_opt_bool("terminal._template_",
		"utf_8_io", 0, 0,
		"Enable I/O in UTF8 for Unicode terminals.");

	add_opt_bool("terminal._template_",
		"restrict_852", 0, 0,
		"Someone who understands this ... ;)) I'm too lazy to think about this now :P.");

	add_opt_bool("terminal._template_",
		"block_cursor", 0, 0,
		"This means that we always move cursor to bottom right corner\n"
		"when done drawing. This is particularly useful when we are\n"
		"using block cursor, as inversed stuff is displayed correctly.");

	add_opt_bool("terminal._template_",
		"colors", 0, 0,
		"If we should use colors.");

	add_opt_codepage("terminal._template_",
		"charset", 0, get_cp_index("us-ascii"),
		"Codepage of charset used for displaying of content on terminal.");



	add_opt_tree("",
		"ui", 0,
		"User interface options.");



	add_opt_tree("ui",
		"colors", 0,
		"Default user interface color settings.");


	/* ========================================================== */
	/* ============= BORING PART (colors) START ================= */
	/* ========================================================== */


	add_opt_tree("ui.colors",
		"color", 0,
		"Color settings for color terminal.");


	add_opt_tree("ui.colors.color",
		"mainmenu", 0,
		"Main menu bar colors.");

	add_opt_tree("ui.colors.color.mainmenu",
		"normal", 0,
		"Unselected menu bar item colors.");

	add_opt_color("ui.colors.color.mainmenu.normal",
		"text", 0, "black",
		"Default text color.");

	add_opt_color("ui.colors.color.mainmenu.normal",
		"background", 0, "white",
		"Default background color.");

	add_opt_tree("ui.colors.color.mainmenu",
		"selected", 0,
		"Unselected menu bar item colors.");

	add_opt_color("ui.colors.color.mainmenu.selected",
		"text", 0, "black",
		"Default text color.");

	add_opt_color("ui.colors.color.mainmenu.selected",
		"background", 0, "green",
		"Default background color.");

	add_opt_tree("ui.colors.color.mainmenu",
		"hotkey", 0,
		"Unselected menu bar item hotkey colors.");

	add_opt_color("ui.colors.color.mainmenu.hotkey",
		"text", 0, "darkred",
		"Default text color.");

	add_opt_color("ui.colors.color.mainmenu.hotkey",
		"background", 0, "white",
		"Default background color.");


	add_opt_tree("ui.colors.color",
		"menu", 0,
		"Menu bar colors.");

	add_opt_tree("ui.colors.color.menu",
		"normal", 0,
		"Unselected menu item colors.");

	add_opt_color("ui.colors.color.menu.normal",
		"text", 0, "black",
		"Default text color.");

	add_opt_color("ui.colors.color.menu.normal",
		"background", 0, "white",
		"Default background color.");

	add_opt_tree("ui.colors.color.menu",
		"selected", 0,
		"Selected menu item colors.");

	add_opt_color("ui.colors.color.menu.selected",
		"text", 0, "black",
		"Default text color.");

	add_opt_color("ui.colors.color.menu.selected",
		"background", 0, "green",
		"Default background color.");

	add_opt_tree("ui.colors.color.menu",
		"hotkey", 0,
		"Unselected menu item hotkey colors.");

	add_opt_color("ui.colors.color.menu.hotkey",
		"text", 0, "darkred",
		"Default text color.");

	add_opt_color("ui.colors.color.menu.hotkey",
		"background", 0, "white",
		"Default background color.");

	add_opt_tree("ui.colors.color.menu",
		"frame", 0,
		"Menu frame colors.");

	add_opt_color("ui.colors.color.menu.frame",
		"text", 0, "black",
		"Default text color.");

	add_opt_color("ui.colors.color.menu.frame",
		"background", 0, "white",
		"Default background color.");


	add_opt_tree("ui.colors.color",
		"dialog", 0,
		"Dialog colors.");

	add_opt_color("ui.colors.color.dialog",
		"background", 0, "white",
		"Dialog generic background color.");

	add_opt_tree("ui.colors.color.dialog",
		"frame", 0,
		"Dialog frame colors.");

	add_opt_color("ui.colors.color.dialog.frame",
		"text", 0, "black",
		"Default text color.");

	add_opt_color("ui.colors.color.dialog.frame",
		"background", 0, "white",
		"Default background color.");

	add_opt_tree("ui.colors.color.dialog",
		"title", 0,
		"Dialog title colors.");

	add_opt_color("ui.colors.color.dialog.title",
		"text", 0, "darkred",
		"Default text color.");

	add_opt_color("ui.colors.color.dialog.title",
		"background", 0, "white",
		"Default background color.");

	add_opt_tree("ui.colors.color.dialog",
		"text", 0,
		"Dialog text colors.");

	add_opt_color("ui.colors.color.dialog.text",
		"text", 0, "black",
		"Default text color.");

	add_opt_color("ui.colors.color.dialog.text",
		"background", 0, "white",
		"Default background color.");

	add_opt_tree("ui.colors.color.dialog",
		"checkbox", 0,
		"Dialog checkbox colors.");

	add_opt_color("ui.colors.color.dialog.checkbox",
		"text", 0, "darkred",
		"Default text color.");

	add_opt_color("ui.colors.color.dialog.checkbox",
		"background", 0, "white",
		"Default background color.");

	add_opt_tree("ui.colors.color.dialog",
		"checkbox-label", 0,
		"Dialog checkbox label colors.");

	add_opt_color("ui.colors.color.dialog.checkbox-label",
		"text", 0, "black",
		"Default text color.");

	add_opt_color("ui.colors.color.dialog.checkbox-label",
		"background", 0, "white",
		"Default background color.");

	add_opt_tree("ui.colors.color.dialog",
		"button", 0,
		"Dialog button colors.");

	add_opt_color("ui.colors.color.dialog.button",
		"text", 0, "white",
		"Default text color.");

	add_opt_color("ui.colors.color.dialog.button",
		"background", 0, "blue",
		"Default background color.");

	add_opt_tree("ui.colors.color.dialog",
		"button-selected", 0,
		"Dialog selected button colors.");

	add_opt_color("ui.colors.color.dialog.button-selected",
		"text", 0, "yellow",
		"Default text color.");

	add_opt_color("ui.colors.color.dialog.button-selected",
		"background", 0, "green",
		"Default background color.");

	add_opt_tree("ui.colors.color.dialog",
		"field", 0,
		"Dialog field colors.");

	add_opt_color("ui.colors.color.dialog.field",
		"text", 0, "white",
		"Default text color.");

	add_opt_color("ui.colors.color.dialog.field",
		"background", 0, "blue",
		"Default background color.");

	add_opt_tree("ui.colors.color.dialog",
		"field-text", 0,
		"Dialog field text colors.");

	add_opt_color("ui.colors.color.dialog.field-text",
		"text", 0, "yellow",
		"Default text color.");

	add_opt_color("ui.colors.color.dialog.field-text",
		"background", 0, "blue",
		"Default background color.");

	add_opt_tree("ui.colors.color.dialog",
		"meter", 0,
		"Dialog meter colors.");

	add_opt_color("ui.colors.color.dialog.meter",
		"text", 0, "white",
		"Default text color.");

	add_opt_color("ui.colors.color.dialog.meter",
		"background", 0, "blue",
		"Default background color.");

	add_opt_tree("ui.colors.color.dialog",
		"shadow", 0,
		"Dialog shadow colors. You need ui.shadows turned on.");

	add_opt_color("ui.colors.color.dialog.shadow",
		"background", 0, "black",
		"Default background color.");


	add_opt_tree("ui.colors.color",
		"title", 0,
		"Title bar colors.");

	add_opt_tree("ui.colors.color.title",
		"title-bar", 0,
		"Generic title bar colors.");

	add_opt_color("ui.colors.color.title.title-bar",
		"text", 0, "black",
		"Default text color.");

	add_opt_color("ui.colors.color.title.title-bar",
		"background", 0, "white",
		"Default background color.");

	add_opt_tree("ui.colors.color.title",
		"title-text", 0,
		"Title bar text colors.");

	add_opt_color("ui.colors.color.title.title-text",
		"text", 0, "black",
		"Default text color.");

	add_opt_color("ui.colors.color.title.title-text",
		"background", 0, "white",
		"Default background color.");


	add_opt_tree("ui.colors.color",
		"status", 0,
		"Status bar colors.");

	add_opt_tree("ui.colors.color.status",
		"status-bar", 0,
		"Generic status bar colors.");

	add_opt_color("ui.colors.color.status.status-bar",
		"text", 0, "black",
		"Default text color.");

	add_opt_color("ui.colors.color.status.status-bar",
		"background", 0, "white",
		"Default background color.");

	add_opt_tree("ui.colors.color.status",
		"status-text", 0,
		"Status bar text colors.");

	add_opt_color("ui.colors.color.status.status-text",
		"text", 0, "black",
		"Default text color.");

	add_opt_color("ui.colors.color.status.status-text",
		"background", 0, "white",
		"Default background color.");


	add_opt_tree("ui.colors",
		"mono", 0,
		"Color settings for non-color terminal.");


	add_opt_tree("ui.colors.mono",
		"mainmenu", 0,
		"Main menu bar colors.");

	add_opt_tree("ui.colors.mono.mainmenu",
		"normal", 0,
		"Unselected menu bar item colors.");

	add_opt_color("ui.colors.mono.mainmenu.normal",
		"text", 0, "black",
		"Default text color.");

	add_opt_color("ui.colors.mono.mainmenu.normal",
		"background", 0, "white",
		"Default background color.");

	add_opt_tree("ui.colors.mono.mainmenu",
		"selected", 0,
		"Unselected menu bar item colors.");

	add_opt_color("ui.colors.mono.mainmenu.selected",
		"text", 0, "gray",
		"Default text color.");

	add_opt_color("ui.colors.mono.mainmenu.selected",
		"background", 0, "black",
		"Default background color.");

	add_opt_tree("ui.colors.mono.mainmenu",
		"hotkey", 0,
		"Unselected menu bar item hotkey colors.");

	add_opt_color("ui.colors.mono.mainmenu.hotkey",
		"text", 0, "black",
		"Default text color.");

	add_opt_color("ui.colors.mono.mainmenu.hotkey",
		"background", 0, "white",
		"Default background color.");


	add_opt_tree("ui.colors.mono",
		"menu", 0,
		"Menu bar colors.");

	add_opt_tree("ui.colors.mono.menu",
		"normal", 0,
		"Unselected menu item colors.");

	add_opt_color("ui.colors.mono.menu.normal",
		"text", 0, "black",
		"Default text color.");

	add_opt_color("ui.colors.mono.menu.normal",
		"background", 0, "white",
		"Default background color.");

	add_opt_tree("ui.colors.mono.menu",
		"selected", 0,
		"Selected menu item colors.");

	add_opt_color("ui.colors.mono.menu.selected",
		"text", 0, "gray",
		"Default text color.");

	add_opt_color("ui.colors.mono.menu.selected",
		"background", 0, "black",
		"Default background color.");

	add_opt_tree("ui.colors.mono.menu",
		"hotkey", 0,
		"Unselected menu item hotkey colors.");

	add_opt_color("ui.colors.mono.menu.hotkey",
		"text", 0, "gray",
		"Default text color.");

	add_opt_color("ui.colors.mono.menu.hotkey",
		"background", 0, "black",
		"Default background color.");

	add_opt_tree("ui.colors.mono.menu",
		"frame", 0,
		"Menu frame colors.");

	add_opt_color("ui.colors.mono.menu.frame",
		"text", 0, "black",
		"Default text color.");

	add_opt_color("ui.colors.mono.menu.frame",
		"background", 0, "white",
		"Default background color.");


	add_opt_tree("ui.colors.mono",
		"dialog", 0,
		"Dialog colors.");

	add_opt_color("ui.colors.mono.dialog",
		"background", 0, "white",
		"Dialog generic background color.");

	add_opt_tree("ui.colors.mono.dialog",
		"frame", 0,
		"Dialog frame colors.");

	add_opt_color("ui.colors.mono.dialog.frame",
		"text", 0, "black",
		"Default text color.");

	add_opt_color("ui.colors.mono.dialog.frame",
		"background", 0, "white",
		"Default background color.");

	add_opt_tree("ui.colors.mono.dialog",
		"title", 0,
		"Dialog title colors.");

	add_opt_color("ui.colors.mono.dialog.title",
		"text", 0, "gray",
		"Default text color.");

	add_opt_color("ui.colors.mono.dialog.title",
		"background", 0, "black",
		"Default background color.");

	add_opt_tree("ui.colors.mono.dialog",
		"text", 0,
		"Dialog text colors.");

	add_opt_color("ui.colors.mono.dialog.text",
		"text", 0, "black",
		"Default text color.");

	add_opt_color("ui.colors.mono.dialog.text",
		"background", 0, "white",
		"Default background color.");

	add_opt_tree("ui.colors.mono.dialog",
		"checkbox", 0,
		"Dialog checkbox colors.");

	add_opt_color("ui.colors.mono.dialog.checkbox",
		"text", 0, "black",
		"Default text color.");

	add_opt_color("ui.colors.mono.dialog.checkbox",
		"background", 0, "white",
		"Default background color.");

	add_opt_tree("ui.colors.mono.dialog",
		"checkbox-label", 0,
		"Dialog checkbox label colors.");

	add_opt_color("ui.colors.mono.dialog.checkbox-label",
		"text", 0, "black",
		"Default text color.");

	add_opt_color("ui.colors.mono.dialog.checkbox-label",
		"background", 0, "white",
		"Default background color.");

	add_opt_tree("ui.colors.mono.dialog",
		"button", 0,
		"Dialog button colors.");

	add_opt_color("ui.colors.mono.dialog.button",
		"text", 0, "black",
		"Default text color.");

	add_opt_color("ui.colors.mono.dialog.button",
		"background", 0, "white",
		"Default background color.");

	add_opt_tree("ui.colors.mono.dialog",
		"button-selected", 0,
		"Dialog selected button colors.");

	add_opt_color("ui.colors.mono.dialog.button-selected",
		"text", 0, "white",
		"Default text color.");

	add_opt_color("ui.colors.mono.dialog.button-selected",
		"background", 0, "black",
		"Default background color.");

	add_opt_tree("ui.colors.mono.dialog",
		"field", 0,
		"Dialog field colors.");

	add_opt_color("ui.colors.mono.dialog.field",
		"text", 0, "gray",
		"Default text color.");

	add_opt_color("ui.colors.mono.dialog.field",
		"background", 0, "black",
		"Default background color.");

	add_opt_tree("ui.colors.mono.dialog",
		"field-text", 0,
		"Dialog field text colors.");

	add_opt_color("ui.colors.mono.dialog.field-text",
		"text", 0, "gray",
		"Default text color.");

	add_opt_color("ui.colors.mono.dialog.field-text",
		"background", 0, "black",
		"Default background color.");

	add_opt_tree("ui.colors.mono.dialog",
		"meter", 0,
		"Dialog meter colors.");

	add_opt_color("ui.colors.mono.dialog.meter",
		"text", 0, "gray",
		"Default text color.");

	add_opt_color("ui.colors.mono.dialog.meter",
		"background", 0, "black",
		"Default background color.");

	add_opt_tree("ui.colors.mono.dialog",
		"shadow", 0,
		"Dialog shadow colors. You need ui.shadows turned on.");

	add_opt_color("ui.colors.mono.dialog.shadow",
		"background", 0, "black",
		"Default background color.");


	add_opt_tree("ui.colors.mono",
		"title", 0,
		"Title bar colors.");

	add_opt_tree("ui.colors.mono.title",
		"title-bar", 0,
		"Generic title bar colors.");

	add_opt_color("ui.colors.mono.title.title-bar",
		"text", 0, "gray",
		"Default text color.");

	add_opt_color("ui.colors.mono.title.title-bar",
		"background", 0, "black",
		"Default background color.");

	add_opt_tree("ui.colors.mono.title",
		"title-text", 0,
		"Title bar text colors.");

	add_opt_color("ui.colors.mono.title.title-text",
		"text", 0, "gray",
		"Default text color.");

	add_opt_color("ui.colors.mono.title.title-text",
		"background", 0, "black",
		"Default background color.");


	add_opt_tree("ui.colors.mono",
		"status", 0,
		"Status bar colors.");

	add_opt_tree("ui.colors.mono.status",
		"status-bar", 0,
		"Generic status bar colors.");

	add_opt_color("ui.colors.mono.status.status-bar",
		"text", 0, "gray",
		"Default text color.");

	add_opt_color("ui.colors.mono.status.status-bar",
		"background", 0, "black",
		"Default background color.");

	add_opt_tree("ui.colors.mono.status",
		"status-text", 0,
		"Status bar text colors.");

	add_opt_color("ui.colors.mono.status.status-text",
		"text", 0, "black",
		"Default text color.");

	add_opt_color("ui.colors.mono.status.status-text",
		"background", 0, "white",
		"Default background color.");


	/* ========================================================== */
	/* ============= BORING PART (colors) END =================== */
	/* ========================================================== */


	add_opt_tree("ui",
		"dialogs", 0,
		"Dialogs-specific appearance and behaviour settings.");

	add_opt_int("ui.dialogs",
		"listbox_min_height", 0, 1, 20, 10,
		"Minimal height of the listbox widget (used ie. for bookmakrs\n"
		"or global history).");

	add_opt_bool("ui.dialogs",
		"shadows", 0, 0,
		"Make dialogs drop shadows (the shadows are solid, you can\n"
		"adjust their color by ui.colors.*.dialog.shadow). You may\n"
		"also want to eliminate the wide borders by adjusting setup.h.");


	add_opt_tree("ui",
		"timer", 0,
		"Timed action after certain interval of user inactivity. Someone can\n"
		"even find this useful, altough you may not believe that.");

#ifdef USE_LEDS
	add_opt_int("ui.timer",
		"enable", 0, 0, 2, 0,
		"Whether to enable the timer or not:\n"
		"0 is don't count down anything\n"
		"1 is count down, but don't show the timer\n"
		"2 is count down and show the timer near LEDs");
#else
	add_opt_int("ui.timer",
		"enable", 0, 0, 2, 0,
		"Whether to enable the timer or not:\n"
		"0 is don't count down anything\n"
		"1 is count down, but don't show the timer\n"
		"2 is count down and show the timer near LEDs (note that this feature is DISABLED)");
#endif

	add_opt_int("ui.timer",
		"duration", 0, 1, 86400, 86400,
		"Length of the inactivity interval. One day should be enough for just\n"
		"everyone (tm).");

	add_opt_string("ui.timer",
		"action", 0, "",
		"Keybinding action to be triggered when timer arrives to zero.");



	add_opt_ptr("ui",
		"language", 0, OPT_LANGUAGE, &current_language,
		"Language of user interface.");

	/* Compatibility alias: added by pasky at 2002-12-01, 0.4pre20.CVS.
	 * Estimated due time: 2003-02-01 */
	add_opt_alias("ui",
		"shadows", 0, "ui.dialogs.shadows",
		"Make dialogs drop shadows (the shadows are solid, you can\n"
		"adjust their color by ui.colors.*.dialog.shadow). You may\n"
		"also want to eliminate the wide borders by adjusting setup.h.");

	add_opt_bool("ui",
		"show_status_bar", 0, 1,
		"Show status bar on the screen.");

	add_opt_bool("ui",
		"show_title_bar", 0, 1,
		"Show title bar on the screen.");

	add_opt_bool("ui",
		"startup_goto_dialog", 0, 0,
		"Pop up goto dialog on startup when there's no homepage set.");

	add_opt_bool("ui",
		"window_title", 0, 1,
		"Whether ELinks window title should be touched if ELinks is\n"
		"being ran in some windowing environment.");



	/* Compatibility alias: added by pasky at 2002-12-10, 0.4pre24.CVS.
	 * Estimated due time: 2003-02-10 */
	add_opt_alias("",
		"config_saving_style", 0, "config.saving_style",
		"Determines what happens when you let ELinks to save options:\n"
		"0 is only values of current options are altered\n"
		"1 is values of current options are altered and missing options\n"
		"     are added at the end of the file\n"
		"2 is the configuration file is rewritten from scratch");

	add_opt_bool("",
		"secure_file_saving", 0, 1,
		"First write data to 'file.tmp', rename to 'file' upon\n"
		"successful finishing this. Note that this relates only to\n"
		"config files, not downloaded files. You may want to disable\n"
		"it, if you want some config file with some exotic permissions.\n"
		"Secure file saving is automagically disabled if file is symlink.");



	/* Commandline options */

	/* Commandline-only options */

	add_opt_bool_tree(&cmdline_options, "",
		"anonymous", 0, 0,
		"Restrict ELinks so that it can run on an anonymous account.\n"
		"No local file browsing, no downloads. Executing of viewers\n"
		"is allowed, but user can't add or modify entries in\n"
		"association table.");

	add_opt_bool_tree(&cmdline_options, "",
		"auto-submit", 0, 0,
		"Go and submit the first form you'll stumble upon.");

	add_opt_int_tree(&cmdline_options, "",
		"base-session", 0, 0, MAXINT, 0,
		"ID of session (ELinks instance) which we want to clone.\n"
		"This is internal ELinks option, you don't want to use it.");

	add_opt_bool_tree(&cmdline_options, "",
		"dump", 0, 0,
		"Write a plain-text version of the given HTML document to\n"
		"stdout.");

	add_opt_alias_tree(&cmdline_options, "",
		"dump-charset", 0, "document.dump.codepage",
		"Codepage used in dump output.");

	add_opt_alias_tree(&cmdline_options, "",
		"dump-width", 0, "document.dump.width",
		"Width of the dump output.");

	add_opt_command_tree(&cmdline_options, "",
		"eval", 0, eval_cmd,
		"Specify elinks.conf config options on the command-line:\n"
		"  -eval 'set protocol.file.allow_special_files = 1'");

	add_opt_command_tree(&cmdline_options, "",
		"?", 0, printhelp_cmd,
		NULL);

	add_opt_command_tree(&cmdline_options, "",
		"h", 0, printhelp_cmd,
		NULL);

	add_opt_command_tree(&cmdline_options, "",
		"help", 0, printhelp_cmd,
		"Print usage help and exit.");

#if 0
	add_opt_command_tree(&cmdline_options, "",
		"help-config", 0, printhelp_cmd,
		"Print help on configuration options and exit.");
#endif

	add_opt_command_tree(&cmdline_options, "",
		"lookup", 0, lookup_cmd,
		"Make lookup for specified host.");

	add_opt_bool_tree(&cmdline_options, "",
		"no-connect", 0, 0,
		"Run ELinks as a separate instance - instead of connecting to\n"
		"existing instance. Note that normally no runtime state files\n"
		"(I mean bookmarks, history and so on) are written to the disk\n"
		"with this option on - see also -touch-files.");

	add_opt_bool_tree(&cmdline_options, "",
		"no-home", 0, 0,
		"Don't attempt to create and/or use home rc directory (~/.elinks).");

	add_opt_int_tree(&cmdline_options, "",
		"session-ring", 0, 0, MAXINT, 0,
		"ID of session ring this ELinks should connect to. The ELinks\n"
		"works in so-called session rings, where all instances of ELinks\n"
		"are interconnected and share same state (cache, bookmarks, cookies\n"
		"and so on). By default, all ELinks instances connect to session\n"
		"ring 0. You can change that behaviour by this switch and form as\n"
		"many session rings as you want. Obviously, if the session-ring with\n"
	        "this number doesn't exist yet, it's created and this ELinks instance\n"
		"will become the master instance (that usually doesn't matter for you\n"
		"as a user much). Note that you usually don't want to use this except\n"
	        "you're a developer and you want to do some testing - if you want the\n"
		"ELinks instances running each one standalone, rather use -no-connect\n"
		"commandline option. Also note that normally no runtime state files\n"
		"are written to the disk with this option on - see also -touch-files.\n");

	add_opt_bool_tree(&cmdline_options, "",
		"source", 0, 0,
		"Write the given HTML document in source form to stdout.");

	add_opt_bool_tree(&cmdline_options, "",
		"stdin", 0, 0,
		"Open stdin as HTML document - it is fully equivalent to:\n"
		" -eval 'set protocol.file.allow_special_files = 1' file:///dev/stdin\n"
		"Use whichever suits you more ;-). Note that reading document from\n"
		"stdin WORKS ONLY WHEN YOU USE -dump OR -source!! (I would like to\n"
		"know why you would use -source -stdin, though ;-)");

	add_opt_bool_tree(&cmdline_options, "",
		"touch-files", 0, 0,
		"Set to 1 to have runtime state files (bookmarks, history, ...)\n"
		"changed even when -no-connect is used; has no effect if not used\n"
		"in connection with the -no-connect commandline option.");

	add_opt_command_tree(&cmdline_options, "",
		"version", 0, version_cmd,
		"Print ELinks version information and exit.");


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
}
