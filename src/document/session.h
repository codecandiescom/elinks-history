/* $Id: session.h,v 1.5 2002/03/28 21:38:51 pasky Exp $ */

#ifndef EL__DOCUMENT_SESSION_H
#define EL__DOCUMENT_SESSION_H

/* We need to declare these first :/. Damn cross-dependencies. */
struct location;
struct session;

#include <document/cache.h>
#include <document/options.h>
#include <document/view.h>
#include <document/html/parser.h>
#include <document/html/renderer.h>
#include <lowlevel/sched.h>
#include <lowlevel/terminal.h>

struct location {
	struct location *next;
	struct location *prev;
	struct list_head frames;
	struct status stat;
	struct view_state vs;
};

#define cur_loc(x) ((struct location *) ((x)->history.next))

/* For map_selected() */
struct link_def {
	unsigned char *link;
	unsigned char *target;
};

/* For ses_*_frame*() */
struct frame {
	struct frame *next;
	struct frame *prev;
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

struct download {
	struct download *next;
	struct download *prev;
	unsigned char *url;
	struct status stat;
	unsigned char *file;
	int last_pos;
	int handle;
	int redirect_cnt;
	unsigned char *prog;
	int prog_flags;
	time_t remotetime;
	struct session *ses;
	struct window *win;
	struct window *ask;
};

/* Stack of all running downloads */
extern struct list_head downloads;

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
	struct session *next;
	struct session *prev;
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
	struct document_setup ds;
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
};

/* extern struct list_head sessions; */

unsigned char *encode_url(unsigned char *);
/* unsigned char *decode_url(unsigned char *); */

unsigned char *subst_file(unsigned char *, unsigned char *);

void free_files(struct session *);

int are_there_downloads();

void free_strerror_buf();
unsigned char *get_err_msg(int);

void print_screen_status(struct session *);
void print_error_dialog(struct session *, struct status *, unsigned char *);

void start_download(struct session *, unsigned char *);
void display_download(struct terminal *, struct download *, struct session *);
int create_download_file(struct terminal *, unsigned char *, int);
void process_file_requests(struct session *);

/* int read_session_info(int, struct session *, void *, int); */
void *create_session_info(int, unsigned char *, int *);

void win_func(struct window *, struct event *, int);

void goto_url_f(struct session *, unsigned char *, unsigned char *);
void goto_url(struct session *, unsigned char *);
void goto_imgmap(struct session *, unsigned char *, unsigned char *, unsigned char *);

void ses_forward(struct session *);
void ses_goto(struct session *, unsigned char *, unsigned char *, int, int,
	      enum session_wtd, unsigned char *, void (*)(struct status *,
		      struct session *), int);

void end_load(struct status *, struct session *);

void abort_loading(struct session *);
void reload(struct session*, int);
void load_frames(struct session *, struct f_data_c *);

struct frame *ses_find_frame(struct session *, unsigned char *);
struct frame *ses_change_frame_url(struct session *, unsigned char *, unsigned char *);

void map_selected(struct terminal *, struct link_def *, struct session *);

void destroy_location(struct location *);
/* void destroy_session(struct session *); */
void destroy_all_sessions();
void abort_all_downloads();

/* Information about the current document */
unsigned char *get_current_url(struct session *, unsigned char *, size_t);
unsigned char *get_current_title(struct session *, unsigned char *, size_t);

unsigned char *get_current_link_url(struct session *, unsigned char *, size_t);

extern struct list_head questions_queue;

void add_questions_entry(void *);

#endif
