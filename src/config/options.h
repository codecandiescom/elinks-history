/* $Id: options.h,v 1.19 2002/05/23 18:50:36 pasky Exp $ */

#ifndef EL__CONFIG_OPTIONS_H
#define EL__CONFIG_OPTIONS_H

#include "document/html/colors.h"
/* Possibly, there should be util/hash.h included as well, but it somehow works
 * without it and I'm glad that 2/3 of ELinks files don't depend on it ;). */

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
	OPT_TERM2,
	OPT_KEYBIND,
	OPT_KEYUNBIND,
	OPT_COLOR,

	OPT_COMMAND,

	OPT_HASH,
};

struct option {
	unsigned char *name;
	enum option_flags flags;
	enum option_type type;
	int min, max;
	void *ptr;
	unsigned char *desc;
};


extern struct hash *root_options;


extern void init_options();
extern void done_options();

extern struct hash *init_options_hash();
extern void free_options_hash(struct hash *);

extern struct option *get_opt_rec(struct hash *, unsigned char *);
extern void *get_opt(struct hash *, unsigned char *);

#define get_opt_int(name) *((int *) get_opt(root_options, name))
#define get_opt_long(name) *((long *) get_opt(root_options, name))
#define get_opt_char(name) *((unsigned char *) get_opt(root_options, name))
#define get_opt_str(name) ((unsigned char *) get_opt(root_options, name))
#define get_opt_ptr(name) ((void *) get_opt(root_options, name))

extern void add_opt_rec(struct hash *, unsigned char *path, struct option *);
extern void add_opt(struct hash *, unsigned char *path, unsigned char *name,
		    enum option_flags flags, enum option_type type,
		    int min, int max, void *ptr,
		    unsigned char *desc);

#define add_opt_bool(path, name, flags, def, desc) do { \
	add_opt(root_options, path, name, flags, OPT_BOOL, 0, 1, mem_alloc(sizeof(int)), desc); \
	get_opt_int(name) = def; } while (0)

#define add_opt_int(path, name, flags, min, max, def, desc) do { \
	add_opt(root_options, path, name, flags, OPT_INT, min, max, mem_alloc(sizeof(int)), desc); \
	get_opt_int(name) = def; } while (0)

#define add_opt_long(path, name, flags, min, max, def, desc) do { \
	add_opt(root_options, path, name, flags, OPT_LONG, min, max, mem_alloc(sizeof(long)), desc); \
	get_opt_long(name) = def; } while (0)

#define add_opt_string(path, name, flags, def, desc) \
	add_opt(root_options, path, name, flags, OPT_STRING, 0, MAX_STR_LEN, stracpy(def), desc);

#define add_opt_codepage(path, name, flags, def, desc) do { \
	add_opt(root_options, path, name, flags, OPT_CODEPAGE, 0, 0, mem_alloc(sizeof(int)), desc); \
	get_opt_int(name) = def; } while (0)

#define add_opt_ptr(path, name, flags, type, def, desc) \
	add_opt(root_options, path, name, flags, type, 0, 0, def, desc);

#define add_opt_void(path, name, flags, type, desc) \
	add_opt(root_options, path, name, flags, type, 0, 0, NULL, desc);

#define add_opt_command(path, name, flags, cmd, desc) \
	add_opt(root_options, path, name, flags, OPT_COMMAND, 0, 0, cmd, desc);

#define add_opt_hash(path, name, flags, desc) \
	add_opt(root_options, path, name, flags, OPT_HASH, 0, 0, init_options_hash(), desc);


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
