/* $Id: session.h,v 1.22 2003/05/27 21:57:53 pasky Exp $ */

#ifndef EL__SCHED_SESSION_H
#define EL__SCHED_SESSION_H

/* We need to declare these first :/. Damn cross-dependencies. */
struct session;

#include "document/cache.h"
#include "document/options.h"
#include "document/html/parser.h"
#include "document/html/renderer.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "sched/sched.h"
#include "util/lists.h"
#include "viewer/text/vs.h"

/* This is used to pass along initial session parameters. */
struct initial_session_info {
	/* The session whose state to copy, -1 is none. */
	int base_session;
	/* The URL we should load immediatelly (or NULL). */
	unsigned char *url;
};

/* For map_selected() */
struct link_def {
	unsigned char *link;
	unsigned char *target;
};

/* For ses_*_frame*() */
struct frame {
	LIST_HEAD(struct frame);

	unsigned char *name;
	int redirect_cnt;
	struct view_state vs;
};

/* For struct session */
struct kbdprefix {
	int rep;
	int rep_num;
	int prefix;
};

/* This should be used only internally */
enum session_wtd {
	WTD_NO,
	WTD_FORWARD,
	WTD_IMGMAP,
	WTD_RELOAD,
	WTD_BACK,
	WTD_UNBACK,
};

struct session {
	LIST_HEAD(struct session);

	struct list_head history;
	struct list_head unhistory;
	struct list_head scrn_frames;
	struct list_head more_files;

	struct status loading;
	struct kbdprefix kbdprefix;
	struct status tq;

	struct window *tab;
	struct cache_entry *tq_ce;
	struct f_data_c *screen;

	unsigned char *wtd_target;
	unsigned char *loading_url;
	unsigned char *goto_position;
	unsigned char *imgmap_href_base;
	unsigned char *imgmap_target_base;
	unsigned char *tq_url;
	unsigned char *tq_goto_position;
	unsigned char *tq_prog;
	unsigned char *dn_url;
	unsigned char *ref_url;
	unsigned char *search_word;
	unsigned char *last_search_word;
	unsigned char *last_title;

	int id;
	int display_timer;
	int reloadlevel;
	int redirect_cnt;
	int tq_prog_flags;
	int search_direction;
	int exit_query;
	int visible_tabs_bar;
	int visible_status_bar;
	int visible_title_bar;

	enum session_wtd wtd;

};

extern struct list_head sessions;


void free_strerror_buf(void);
unsigned char *get_err_msg(int);

void print_screen_status(struct session *);
void print_error_dialog(struct session *, struct status *, unsigned char *);

void process_file_requests(struct session *);

void *create_session_info(int, unsigned char *, int *);
struct initial_session_info *decode_session_info(const void *);
struct session *create_basic_session(struct window *);

void init_bars_status(struct session *, int *, struct document_options *);

void tabwin_func(struct window *, struct event *, int);

void goto_url_frame_reload(struct session *, unsigned char *, unsigned char *);
void goto_url_frame(struct session *, unsigned char *, unsigned char *);
void goto_url(struct session *, unsigned char *);
void goto_url_with_hook(struct session *, unsigned char *);
void goto_imgmap(struct session *, unsigned char *, unsigned char *, unsigned char *);

void ses_forward(struct session *);
void ses_goto(struct session *, unsigned char *, unsigned char *, int,
	      enum cache_mode, enum session_wtd, unsigned char *,
	      void (*)(struct status *, struct session *), int);

void end_load(struct status *, struct session *);
void doc_end_load(struct status *, struct session *);

void abort_loading(struct session *, int);
void reload(struct session *, enum cache_mode);
void load_frames(struct session *, struct f_data_c *);

struct frame *ses_find_frame(struct session *, unsigned char *);
struct frame *ses_change_frame_url(struct session *, unsigned char *, unsigned char *);

void map_selected(struct terminal *, struct link_def *, struct session *);

/* void destroy_session(struct session *); */
void destroy_all_sessions(void);

void free_files(struct session *);

void display_timer(struct session *ses);

/* Information about the current document */
unsigned char *get_current_url(struct session *, unsigned char *, size_t);
unsigned char *get_current_title(struct session *, unsigned char *, size_t);

struct link *get_current_link(struct session *ses);
unsigned char *get_current_link_url(struct session *, unsigned char *, size_t);
unsigned char *get_current_link_name(struct session *, unsigned char *, size_t);

extern struct list_head questions_queue;

void add_questions_entry(void *);

#endif
