/* $Id: options.h,v 1.68 2003/10/20 15:22:32 zas Exp $ */

#ifndef EL__CONFIG_OPTIONS_H
#define EL__CONFIG_OPTIONS_H

/* #include "bfu/listbox.h" */
struct listbox_item;

#include "util/color.h"
#include "util/lists.h"
#include "util/memory.h"
#include "util/string.h"

/* TODO: We should provide some generic mechanism for options caching. */

/* This in particular is *not* a nice occassion for putting in any funny
 * replacement as we want our keyboards to survive debugging sessions, which
 * frequently involve typing this substituted token all the time. --pasky */
#define option option_cygwin_workaround


enum option_flags {
	/* bitmask */
	/* The option is hidden - it serves for internal purposes, never
	 * is read, never is written, never is displayed, never is crawled
	 * through etc. */
	OPT_HIDDEN = 1,
	/* For OPT_TREE, automatically create missing hiearchy piece just
	 * under this category when adding an option. The 'template' for
	 * the added hiearchy piece (category) is stored as "_template_"
	 * category. */
	OPT_AUTOCREATE = 2,
	/* This is used just for marking various options for some very dark,
	 * nasty and dirty purposes. This watermarking should be kept inside
	 * some very closed and clearly bounded piece of ELinks module, not
	 * spreaded along whole ELinks code, and you should clear it everytime
	 * when sneaking outside of the module (except some trivial common
	 * utility functions). Basically, you don't want to use this flag
	 * normally ;). It doesn't affect how the option is handled by common
	 * option handling functions in any way. */
	OPT_WATERMARK = 4,
	/* This is used to mark options modified after the last save. That's
	 * being useful if you want to save only the options whose value
	 * changed. */
	OPT_TOUCHED = 8,
	/* This is used to mark options which are in config_options. If set
	 * on the tree argument to add_opt, it will create the listbox (options
	 * manager) item for the option. */
	OPT_LISTBOX = 16,
};

enum option_type {
	OPT_BOOL,
	OPT_INT,
	OPT_LONG,
	OPT_STRING,

	OPT_CODEPAGE,
	OPT_LANGUAGE,
	OPT_COLOR,

	OPT_COMMAND,

	OPT_ALIAS, /* ptr is "real" option name */

	OPT_TREE,
};

struct session;

#if 0
struct option;

/* TODO: Sometime in the distant future somene will use this or similar
 * approach to store option values. It could be you? --jonas */
union option_value {
	/* Keep first to makes options_root initialization possible. */
	struct list_head *tree;

	/* Used by OPT_BOOL, OPT_INT, OPT_LONG, OPT_CODEPAGE and
	 * OPT_LANGUAGE */
	long number;

	/* The rest is basicly OPT_#{toupper(membername)} */

	color_t color;
	unsigned char *(*command)(struct option *, unsigned char ***, int *);

	/* Each string option get allocated MAX_STR_LEN bytes. Used by
	 * OPT_STRING and OPT_ALIAS. */
	unsigned char *string;
};
#endif

struct option {
	LIST_HEAD(struct option);

	unsigned char *name;
	enum option_flags flags;
	enum option_type type;
	int min, max;
	void *ptr;
	unsigned char *desc;
	unsigned char *capt;

	/* To be called when the option (or sub-option if it's a tree) is
	 * changed. If it returns zero, we will continue descending the options
	 * tree checking for change handlers. */
	int (*change_hook)(struct session *, struct option *current, struct option *changed);

	/* This is indeed maintained by bookmarks.c, not dialogs.c; much easier
	 * and simpler. */
	struct listbox_item *box_item;
};

extern struct option options_root;
extern struct option *config_options;
extern struct option *cmdline_options;

extern struct list_head config_option_box_items;
extern struct list_head option_boxes;


extern void init_options(void);
extern void done_options(void);

extern struct list_head *init_options_tree(void);
extern void unmark_options_tree(struct list_head *);

extern void smart_config_string(struct string *, int, int, struct list_head *, unsigned char *, int,
				void (*)(struct string *, struct option *, unsigned char *, int, int, int, int));

extern struct option *copy_option(struct option *);
extern void delete_option(struct option *);
extern void free_option_value(struct option *);

/* Shitload of various incredible macro combinations and other unusable garbage
 * follows. Have fun. */

/* Basically, for main hiearchy addressed from root (almost always) you want to
 * use get_opt_type() and add_opt_type(). For command line options, you want to
 * use get_opt_type_tree(cmdline_options). */

extern struct option *get_opt_rec(struct option *, unsigned char *);
extern struct option *get_opt_rec_real(struct option *, unsigned char *);
#ifdef DEBUG
extern void *get_opt_(unsigned char *, int, struct option *, unsigned char *);
#define get_opt(tree, name) get_opt_(__FILE__, __LINE__, tree, name)
#else
extern void *get_opt_(struct option *, unsigned char *);
#define get_opt(tree, name) get_opt_(tree, name)
#endif

#define get_opt_bool_tree(tree, name) *((int *) get_opt(tree, name))
#define get_opt_int_tree(tree, name) *((int *) get_opt(tree, name))
#define get_opt_long_tree(tree, name) *((long *) get_opt(tree, name))
#define get_opt_char_tree(tree, name) *((unsigned char *) get_opt(tree, name))
#define get_opt_str_tree(tree, name) ((unsigned char *) get_opt(tree, name))
#define get_opt_ptr_tree(tree, name) ((void *) get_opt(tree, name))
#define get_opt_color_tree(tree, name) (*(color_t *) get_opt(tree, name))

#define get_opt_bool(name) get_opt_bool_tree(config_options, name)
#define get_opt_int(name) get_opt_int_tree(config_options, name)
#define get_opt_long(name) get_opt_long_tree(config_options, name)
#define get_opt_char(name) get_opt_char_tree(config_options, name)
#define get_opt_str(name) get_opt_str_tree(config_options, name)
#define get_opt_ptr(name) get_opt_ptr_tree(config_options, name)
#define get_opt_color(name) get_opt_color_tree(config_options, name)


extern struct option *add_opt(struct option *, unsigned char *, unsigned char *,
			      unsigned char *, enum option_flags, enum option_type,
			      int, int, void *, unsigned char *);

/* Hack which permit to disable option descriptions, to reduce elinks binary size.
 * It may of some use for people wanting a very small static non-i18n elinks binary,
 * at time of writing gain is over 25Kbytes. --Zas */
#ifndef ELINKS_SMALL
#define DESC(x) (x)
#else
#define DESC(x) ((unsigned char *) "")
#endif

#define add_opt_bool_tree(tree, path, capt, name, flags, def, desc) do { \
	int *ptr = mem_alloc(sizeof(int)); \
	add_opt(tree, path, capt, name, flags, OPT_BOOL, 0, 1, ptr, DESC(desc)); \
	*ptr = def; } while (0)

#define add_opt_int_tree(tree, path, capt, name, flags, min, max, def, desc) do { \
	int *ptr = mem_alloc(sizeof(int)); \
	add_opt(tree, path, capt, name, flags, OPT_INT, min, max, ptr, DESC(desc)); \
	*ptr = def; } while (0)

#define add_opt_long_tree(tree, path, capt, name, flags, min, max, def, desc) do { \
	long *ptr = mem_alloc(sizeof(long)); \
	add_opt(tree, path, capt, name, flags, OPT_LONG, min, max, ptr, DESC(desc)); \
	*ptr = def; } while (0)

#define add_opt_str_tree(tree, path, capt, name, flags, def, desc) do { \
	unsigned char *ptr = mem_alloc(MAX_STR_LEN); \
	add_opt(tree, path, capt, name, flags, OPT_STRING, 0, MAX_STR_LEN, ptr, DESC(desc)); \
	safe_strncpy(ptr, def, MAX_STR_LEN); } while (0)

#define add_opt_codepage_tree(tree, path, capt, name, flags, def, desc) do { \
	int *ptr = mem_alloc(sizeof(int)); \
	add_opt(tree, path, capt, name, flags, OPT_CODEPAGE, 0, 0, ptr, DESC(desc)); \
	*ptr = def; } while (0)

extern int color_set(struct option *, unsigned char *); /* XXX */

#define add_opt_color_tree(tree, path, capt, name, flags, def, desc) do { \
	color_t *ptr = mem_alloc(sizeof(color_t)); \
	color_set(add_opt(tree, path, capt, name, flags, OPT_COLOR, 0, 0, ptr, DESC(desc)), def); \
	} while (0)

#define add_opt_ptr_tree(tree, path, capt, name, flags, type, def, desc) \
	add_opt(tree, path, capt, name, flags, type, 0, 0, def, DESC(desc));

#define add_opt_void_tree(tree, path, capt, name, flags, type, desc) \
	add_opt(tree, path, capt, name, flags, type, 0, 0, NULL, DESC(desc));

#define add_opt_command_tree(tree, path, capt, name, flags, cmd, desc) \
	add_opt(tree, path, capt, name, flags, OPT_COMMAND, 0, 0, cmd, DESC(desc));

#define add_opt_alias_tree(tree, path, capt, name, flags, def, desc) do { \
	unsigned char *ptr = mem_alloc(MAX_STR_LEN); \
	add_opt(tree, path, capt, name, flags, OPT_ALIAS, 0, MAX_STR_LEN, ptr, DESC(desc)); \
	safe_strncpy(ptr, def, MAX_STR_LEN); } while (0)

#define add_opt_tree_tree(tree, path, capt, name, flags, desc) \
	add_opt(tree, path, capt, name, flags, OPT_TREE, 0, 0, init_options_tree(), DESC(desc));

#define add_opt_bool(path, capt, name, flags, def, desc) \
	add_opt_bool_tree(config_options, path, capt, name, flags, def, DESC(desc))

#define add_opt_int(path, capt, name, flags, min, max, def, desc) \
	add_opt_int_tree(config_options, path, capt, name, flags, min, max, def, DESC(desc))

#define add_opt_long(path, capt, name, flags, min, max, def, desc) \
	add_opt_long_tree(config_options, path, capt, name, flags, min, max, def, DESC(desc))

#define add_opt_str(path, capt, name, flags, def, desc) \
	add_opt_str_tree(config_options, path, capt, name, flags, def, DESC(desc))

#define add_opt_codepage(path, capt, name, flags, def, desc) \
	add_opt_codepage_tree(config_options, path, capt, name, flags, def, DESC(desc))

#define add_opt_color(path, capt, name, flags, def, desc) \
	add_opt_color_tree(config_options, path, capt, name, flags, def, DESC(desc))

#define add_opt_ptr(path, capt, name, flags, type, def, desc) \
	add_opt_ptr_tree(config_options, path, capt, name, flags, type, def, DESC(desc))

#define add_opt_void(path, capt, name, flags, type, desc) \
	add_opt_void_tree(config_options, path, capt, name, flags, type, DESC(desc))

#define add_opt_command(path, capt, name, flags, cmd, desc) \
	add_opt_command_tree(config_options, path, capt, name, flags, cmd, DESC(desc))

#define add_opt_alias(path, capt, name, flags, def, desc) \
	add_opt_alias_tree(config_options, path, capt, name, flags, def, DESC(desc))

#define add_opt_tree(path, capt, name, flags, desc) \
	add_opt_tree_tree(config_options, path, capt, name, flags, DESC(desc))

/* TODO: We need to do *something* with this ;). */

enum cookies_accept {
	COOKIES_ACCEPT_NONE,
	COOKIES_ACCEPT_ASK,
	COOKIES_ACCEPT_ALL
};

enum referer {
	REFERER_NONE,
	REFERER_SAME_URL,
	REFERER_FAKE,
	REFERER_TRUE,
};

#endif
