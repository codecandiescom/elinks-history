/* $Id: default.h,v 1.2 2002/03/17 13:54:12 pasky Exp $ */

#ifndef EL__DEFAULT_H
#define EL__DEFAULT_H

#include <document/session.h>
#include <document/html/colors.h>
#include <lowlevel/terminal.h>

#define option option_dirty_workaround_for_name_clash_with_include_on_cygwin

struct option {
	unsigned char *cmd_name;
	unsigned char *cfg_name;
	unsigned char *(*rd_cmd)(struct option *, unsigned char ***, int *);
	unsigned char *(*rd_cfg)(struct option *, unsigned char *);
	void (*wr_cfg)(struct option *, unsigned char **, int *);
	int min, max;
	void *ptr;
	unsigned char *desc;
};

void init_home();
unsigned char *parse_options(int, unsigned char *[]);
unsigned char *get_token(unsigned char **line);
void load_config();
void write_config(struct terminal *);
void write_html_config(struct terminal *);
void end_config();

int load_url_history();
int save_url_history();

extern int anonymous;
extern unsigned char user_agent[];

extern unsigned char system_name[];

extern unsigned char *links_home;
extern int first_use;

/* extern int created_home; */

extern int no_connect;
extern int base_session;

#define D_DUMP		1
#define D_SOURCE	2
extern int dmp;
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

extern int async_lookup;
extern int download_utime;
extern int max_connections;
extern int max_connections_to_host;
extern int max_tries;
extern int receive_timeout;
extern int unrestartable_receive_timeout;

extern int keep_unhistory;

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
