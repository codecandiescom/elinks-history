/* $Id: options.h,v 1.24 2002/05/25 21:40:51 pasky Exp $ */

#ifndef EL__CONFIG_OPTIONS_H
#define EL__CONFIG_OPTIONS_H

#include "document/html/colors.h"
#include "links.h" /* lists stuff */ /* MAX_STR_LEN, safe_strncpy() */

#define option option_dirty_workaround_for_name_clash_with_include_on_cygwin


enum option_flags {
	/* bitmask */
	OPT_CMDLINE = 1,
	OPT_CFGFILE = 2,
};

enum option_type {
	OPT_BOOL,
	OPT_INT,
	OPT_LONG,
	OPT_STRING,

	OPT_CODEPAGE,
	OPT_LANGUAGE,
	OPT_MIME_TYPE,
	OPT_EXTENSION,
	OPT_PROGRAM,
	OPT_TERM,
	OPT_KEYBIND,
	OPT_KEYUNBIND,
	OPT_COLOR,

	OPT_COMMAND,

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


extern void init_options();
extern void done_options();

extern struct list_head *init_options_tree();
extern void free_options_tree(struct list_head *);

extern struct option *get_opt_rec(struct list_head *, unsigned char *);
extern void *get_opt_(unsigned char *, int, struct list_head *, unsigned char *);
#define get_opt(tree, name) get_opt_(__FILE__, __LINE__, tree, name)

#define get_opt_int(name) *((int *) get_opt(root_options, name))
#define get_opt_long(name) *((long *) get_opt(root_options, name))
#define get_opt_char(name) *((unsigned char *) get_opt(root_options, name))
#define get_opt_str(name) ((unsigned char *) get_opt(root_options, name))
#define get_opt_ptr(name) ((void *) get_opt(root_options, name))

extern void add_opt_rec(struct list_head *, unsigned char *path, struct option *);
extern void add_opt(struct list_head *, unsigned char *path, unsigned char *name,
		    enum option_flags flags, enum option_type type,
		    int min, int max, void *ptr,
		    unsigned char *desc);

#define add_opt_bool(path, name, flags, def, desc) do { \
	int *ptr = mem_alloc(sizeof(int)); \
	add_opt(root_options, path, name, flags, OPT_BOOL, 0, 1, ptr, desc); \
	*ptr = def; } while (0)

#define add_opt_int(path, name, flags, min, max, def, desc) do { \
	int *ptr = mem_alloc(sizeof(int)); \
	add_opt(root_options, path, name, flags, OPT_INT, min, max, ptr, desc); \
	*ptr = def; } while (0)

#define add_opt_long(path, name, flags, min, max, def, desc) do { \
	long *ptr = mem_alloc(sizeof(long)); \
	add_opt(root_options, path, name, flags, OPT_LONG, min, max, ptr, desc); \
	*ptr = def; } while (0)

#define add_opt_string(path, name, flags, def, desc) do { \
	unsigned char *ptr = mem_alloc(MAX_STR_LEN); \
	add_opt(root_options, path, name, flags, OPT_STRING, 0, MAX_STR_LEN, ptr, desc); \
	safe_strncpy(ptr, def, MAX_STR_LEN); } while (0)

#define add_opt_codepage(path, name, flags, def, desc) do { \
	int *ptr = mem_alloc(sizeof(int)); \
	add_opt(root_options, path, name, flags, OPT_CODEPAGE, 0, 0, ptr, desc); \
	*ptr = def; } while (0)

#define add_opt_ptr(path, name, flags, type, def, desc) \
	add_opt(root_options, path, name, flags, type, 0, 0, def, desc);

#define add_opt_void(path, name, flags, type, desc) \
	add_opt(root_options, path, name, flags, type, 0, 0, NULL, desc);

#define add_opt_command(path, name, flags, cmd, desc) \
	add_opt(root_options, path, name, flags, OPT_COMMAND, 0, 0, cmd, desc);

#define add_opt_tree(path, name, flags, desc) \
	add_opt(root_options, path, name, flags, OPT_TREE, 0, 0, init_options_tree(), desc);


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
