/* $Id: session.h,v 1.9 2003/05/04 17:25:56 pasky Exp $ */

#ifndef EL__SCHED_SESSION_H
#define EL__SCHED_SESSION_H

/* We need to declare these first :/. Damn cross-dependencies. */
struct session;

#include "document/cache.h"
#include "document/options.h"
#include "document/html/parser.h"
#include "document/html/renderer.h"
#include "terminal/terminal.h"
#include "sched/sched.h"
#include "util/lists.h"
#include "viewer/text/vs.h"

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
	struct terminal *term;
	struct window *win;
	int id;
	struct f_data_c *screen;
	struct list_head scrn_frames;
	struct status loading;
	enum session_wtd wtd;
	unsigned char *wtd_target;
	unsigned char *loading_url;
	int display_timer;
	struct list_head more_files;
	unsigned char *goto_position;
	unsigned char *imgmap_href_base;
	unsigned char *imgmap_target_base;
	struct kbdprefix kbdprefix;
	int reloadlevel;
	int redirect_cnt;
	struct status tq;
	unsigned char *tq_url;
	struct cache_entry *tq_ce;
	unsigned char *tq_goto_position;
	unsigned char *tq_prog;
	int tq_prog_flags;
	unsigned char *dn_url;
	unsigned char *ref_url;
	unsigned char *search_word;
	unsigned char *last_search_word;
	int search_direction;
	int exit_query;
	int visible_tab_bar;
	unsigned char *last_title;
};

extern struct list_head sessions;

unsigned char *encode_url(unsigned char *);
unsigned char *decode_url(unsigned char *);

void add_xnum_to_str(unsigned char **, int *, int);
void add_time_to_str(unsigned char **, int *, ttime);

void free_strerror_buf();
unsigned char *get_err_msg(int);

void print_screen_status(struct session *);
void print_error_dialog(struct session *, struct status *, unsigned char *);

void process_file_requests(struct session *);

/* int read_session_info(int, struct session *, void *, int); */
void *create_session_info(int, unsigned char *, int *);
struct session *create_basic_session(struct window *);

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
void destroy_all_sessions();

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
