/* $Id: options.h,v 1.5 2002/04/28 18:03:41 pasky Exp $ */

#ifndef EL__CONFIG_OPTIONS_H
#define EL__CONFIG_OPTIONS_H

#include <document/options.h>
#include <document/html/colors.h>

#define option option_dirty_workaround_for_name_clash_with_include_on_cygwin

enum option_flags {
	/* bitmask */
	OPT_CMDLINE = 1,
	OPT_CFGFILE = 2,
};

struct option {
	unsigned char *name;
	enum option_flags flags;
	unsigned char *(*rd_cmd)(struct option *, unsigned char ***, int *);
	unsigned char *(*rd_cfg)(struct option *, unsigned char *);
	void (*wr_cfg)(struct option *, unsigned char **, int *);
	int min, max;
	void *ptr;
	unsigned char *desc;
};

extern struct option links_options[];
extern struct option html_options[];
extern struct option *all_options[];


extern struct option *get_opt_rec(unsigned char *);

extern void *get_opt(unsigned char *);

#define get_opt_int(name) *((int *) get_opt(name))
#define get_opt_long(name) *((long *) get_opt(name))
#define get_opt_char(name) *((unsigned char *) get_opt(name))
#define get_opt_str(name) ((unsigned char *) get_opt(name))

extern unsigned char *cmd_name(unsigned char *);


extern int anonymous;
extern unsigned char user_agent[];

extern int no_connect;
extern int base_session;

enum dump_type {
	D_NONE,
	D_DUMP,
	D_SOURCE,
};

extern enum dump_type dmp;
extern int dump_width;

enum cookies_accept {
	COOKIES_ACCEPT_NONE,
	COOKIES_ACCEPT_ASK,
	COOKIES_ACCEPT_ALL
};

extern enum cookies_accept cookies_accept;
extern int cookies_save;
extern int cookies_resave;
extern int cookies_paranoid_security;

extern int secure_save;

extern int async_lookup;
extern int download_utime;
extern int max_connections;
extern int max_connections_to_host;
extern int max_tries;
extern int receive_timeout;
extern int unrestartable_receive_timeout;

extern int keep_unhistory;

extern int enable_global_history;

extern struct document_setup dds;

extern int max_format_cache_entries;
extern long memory_cache_size;

extern struct rgb default_fg;
extern struct rgb default_bg;
extern struct rgb default_link;
extern struct rgb default_vlink;

extern int color_dirs;

extern int show_status_bar;
extern int show_title_bar;

extern int form_submit_auto;
extern int form_submit_confirm;
extern int accesskey_enter;
extern int accesskey_priority;
extern int links_wraparound;

extern int allow_special_files;

enum referer {
	REFERER_NONE,
	REFERER_SAME_URL,
	REFERER_FAKE,
	REFERER_TRUE,
};

extern enum referer referer;
extern unsigned char fake_referer[];
extern unsigned char http_proxy[];
extern unsigned char ftp_proxy[];
extern unsigned char no_proxy_for[];
extern unsigned char download_dir[];
extern unsigned char default_mime_type[];

extern unsigned char proxy_user[];
extern unsigned char proxy_passwd[];

extern int startup_goto_dialog;

struct http_bugs {
	int http10;
	int allow_blacklist;
	int bug_302_redirect;
	int bug_post_no_keepalive;
};

extern struct http_bugs http_bugs;

extern unsigned char default_anon_pass[];

#endif
