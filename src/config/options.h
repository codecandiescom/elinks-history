/* $Id: options.h,v 1.11 2002/05/18 23:01:33 pasky Exp $ */

#ifndef EL__CONFIG_OPTIONS_H
#define EL__CONFIG_OPTIONS_H

#include "document/options.h"
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
};

struct option {
	unsigned char *name;
	enum option_flags flags;
	enum option_type type;
	int min, max;
	void *ptr;
	unsigned char *desc;
};


struct option_type_info {
	unsigned char *(*rd_cmd)(struct option *, unsigned char ***, int *);
	unsigned char *(*rd_cfg)(struct option *, unsigned char *);
	void (*wr_cfg)(struct option *, unsigned char **, int *);
	unsigned char *help_str;
};

extern struct option_type_info option_types[];


extern struct hash *links_options;
extern struct hash *html_options;
extern struct hash *all_options[];


extern void init_options();
extern void done_options();

extern struct option *get_opt_rec(struct hash *, unsigned char *);
extern void *get_opt(struct hash *, unsigned char *);

#define get_opt_int(name) *((int *) get_opt(links_options, name))
#define get_opt_long(name) *((long *) get_opt(links_options, name))
#define get_opt_char(name) *((unsigned char *) get_opt(links_options, name))
#define get_opt_str(name) ((unsigned char *) get_opt(links_options, name))
#define get_opt_ptr(name) ((void *) get_opt(links_options, name))

extern void add_opt_rec(struct hash *, struct option *);

extern unsigned char *cmd_name(unsigned char *);
extern unsigned char *opt_name(unsigned char *);


enum cookies_accept {
	COOKIES_ACCEPT_NONE,
	COOKIES_ACCEPT_ASK,
	COOKIES_ACCEPT_ALL
};

extern struct document_setup dds;

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

struct http_bugs {
	int http10;
	int allow_blacklist;
	int bug_302_redirect;
	int bug_post_no_keepalive;
};

#endif
