/* $Id: options.h,v 1.27 2002/06/09 14:53:22 pasky Exp $ */

#ifndef EL__CONFIG_OPTIONS_H
#define EL__CONFIG_OPTIONS_H

#include "document/html/colors.h"
#include "links.h" /* lists stuff */ /* MAX_STR_LEN, safe_strncpy() */

#define option option_dirty_workaround_for_name_clash_with_include_on_cygwin


enum option_flags {
	/* bitmask */
	/* The option is hidden - it serves for internal purposes, never
	 * is read, never is written, never is displayed, never is crawled
	 * through etc. */
	OPT_HIDDEN = 1,
	/* For OPT_TREE, automatically create missing hiearchy piece just
	 * under this category when adding an option. The 'template' for
	 * the added hiearchy piece (category) is stored as "#template#"
	 * category and should have OPT_HIDDEN. */
	OPT_AUTOCREATE = 2,
};

enum option_type {
	OPT_BOOL,
	OPT_INT,
	OPT_LONG,
	OPT_STRING,

	OPT_CODEPAGE,
	OPT_LANGUAGE,
#if 0
	OPT_MIME_TYPE,
	OPT_EXTENSION,
	OPT_TERM,
	OPT_KEYBIND,
	OPT_KEYUNBIND,
#endif
	OPT_PROGRAM,
	OPT_COLOR,

	OPT_COMMAND,

	OPT_ALIAS, /* ptr is "real" option name */

	OPT_TREE,
};

struct option {
	struct option *next;
	struct option *prev;

	unsigned char *name;
	enum option_flags flags;
	enum option_type type;
	int min, max;
	void *ptr;
	unsigned char *desc;
};


extern struct list_head *root_options;
extern struct list_head *cmdline_options;


extern void init_options();
extern void done_options();

extern struct list_head *init_options_tree();
extern void free_options_tree(struct list_head *);

extern struct option *copy_option(struct option *);

/* Shitload of various incredible macro combinations and other unusable garbage
 * follows. Have fun. */

/* Basically, for main hiearchy addressed from root (almost always) you want to
 * use get_opt_type() and add_opt_type(). For command line options, you want to
 * use get_opt_type_tree(cmdline_options). */

extern struct option *get_opt_rec(struct list_head *, unsigned char *);
extern void *get_opt_(unsigned char *, int, struct list_head *, unsigned char *);
#define get_opt(tree, name) get_opt_(__FILE__, __LINE__, tree, name)

#define get_opt_bool_tree(tree, name) *((int *) get_opt(tree, name))
#define get_opt_int_tree(tree, name) *((int *) get_opt(tree, name))
#define get_opt_long_tree(tree, name) *((long *) get_opt(tree, name))
#define get_opt_char_tree(tree, name) *((unsigned char *) get_opt(tree, name))
#define get_opt_str_tree(tree, name) ((unsigned char *) get_opt(tree, name))
#define get_opt_ptr_tree(tree, name) ((void *) get_opt(tree, name))

#define get_opt_bool(name) get_opt_bool_tree(root_options, name)
#define get_opt_int(name) get_opt_int_tree(root_options, name)
#define get_opt_long(name) get_opt_long_tree(root_options, name)
#define get_opt_char(name) get_opt_char_tree(root_options, name)
#define get_opt_str(name) get_opt_str_tree(root_options, name)
#define get_opt_ptr(name) get_opt_ptr_tree(root_options, name)


extern void add_opt_rec(struct list_head *, unsigned char *path, struct option *);
extern void add_opt(struct list_head *, unsigned char *path, unsigned char *name,
		    enum option_flags flags, enum option_type type,
		    int min, int max, void *ptr,
		    unsigned char *desc);

#define add_opt_bool_tree(tree, path, name, flags, def, desc) do { \
	int *ptr = mem_alloc(sizeof(int)); \
	add_opt(tree, path, name, flags, OPT_BOOL, 0, 1, ptr, desc); \
	*ptr = def; } while (0)

#define add_opt_int_tree(tree, path, name, flags, min, max, def, desc) do { \
	int *ptr = mem_alloc(sizeof(int)); \
	add_opt(tree, path, name, flags, OPT_INT, min, max, ptr, desc); \
	*ptr = def; } while (0)

#define add_opt_long_tree(tree, path, name, flags, min, max, def, desc) do { \
	long *ptr = mem_alloc(sizeof(long)); \
	add_opt(tree, path, name, flags, OPT_LONG, min, max, ptr, desc); \
	*ptr = def; } while (0)

#define add_opt_string_tree(tree, path, name, flags, def, desc) do { \
	unsigned char *ptr = mem_alloc(MAX_STR_LEN); \
	add_opt(tree, path, name, flags, OPT_STRING, 0, MAX_STR_LEN, ptr, desc); \
	safe_strncpy(ptr, def, MAX_STR_LEN); } while (0)

#define add_opt_codepage_tree(tree, path, name, flags, def, desc) do { \
	int *ptr = mem_alloc(sizeof(int)); \
	add_opt(tree, path, name, flags, OPT_CODEPAGE, 0, 0, ptr, desc); \
	*ptr = def; } while (0)

#define add_opt_ptr_tree(tree, path, name, flags, type, def, desc) \
	add_opt(tree, path, name, flags, type, 0, 0, def, desc);

#define add_opt_void_tree(tree, path, name, flags, type, desc) \
	add_opt(tree, path, name, flags, type, 0, 0, NULL, desc);

#define add_opt_command_tree(tree, path, name, flags, cmd, desc) \
	add_opt(tree, path, name, flags, OPT_COMMAND, 0, 0, cmd, desc);

#define add_opt_alias_tree(tree, path, name, flags, def, desc) do { \
	unsigned char *ptr = mem_alloc(MAX_STR_LEN); \
	add_opt(tree, path, name, flags, OPT_ALIAS, 0, MAX_STR_LEN, ptr, desc); \
	safe_strncpy(ptr, def, MAX_STR_LEN); } while (0)

#define add_opt_tree_tree(tree, path, name, flags, desc) \
	add_opt(tree, path, name, flags, OPT_TREE, 0, 0, init_options_tree(), desc);

#define add_opt_bool(path, name, flags, def, desc) add_opt_bool_tree(root_options, path, name, flags, def, desc)
#define add_opt_int(path, name, flags, min, max, def, desc) add_opt_int_tree(root_options, path, name, flags, min, max, def, desc)
#define add_opt_long(path, name, flags, min, max, def, desc) add_opt_long_tree(root_options, path, name, flags, min, max, def, desc)
#define add_opt_string(path, name, flags, def, desc) add_opt_string_tree(root_options, path, name, flags, def, desc)
#define add_opt_codepage(path, name, flags, def, desc) add_opt_codepage_tree(root_options, path, name, flags, def, desc)
#define add_opt_ptr(path, name, flags, type, def, desc) add_opt_ptr_tree(root_options, path, name, flags, type, def, desc)
#define add_opt_void(path, name, flags, type, desc) add_opt_void_tree(root_options, path, name, flags, type, desc)
#define add_opt_command(path, name, flags, cmd, desc) add_opt_command_tree(root_options, path, name, flags, cmd, desc)
#define add_opt_alias(path, name, flags, def, desc) add_opt_alias_tree(root_options, path, name, flags, def, desc)
#define add_opt_tree(path, name, flags, desc) add_opt_tree_tree(root_options, path, name, flags, desc)


extern unsigned char *cmd_name(unsigned char *);
extern unsigned char *opt_name(unsigned char *);


enum cookies_accept {
	COOKIES_ACCEPT_NONE,
	COOKIES_ACCEPT_ASK,
	COOKIES_ACCEPT_ALL
};

extern struct rgb default_fg;
extern struct rgb default_bg;
extern struct rgb default_link;
extern struct rgb default_vlink;

enum referer {
	REFERER_NONE,
	REFERER_SAME_URL,
	REFERER_FAKE,
	REFERER_TRUE,
};

#endif
