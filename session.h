/* $Id: session.h,v 1.3 2002/03/16 22:03:09 pasky Exp $ */

#ifndef EL__SESSION_H
#define EL__SESSION_H

#include "cache.h"
#include "html.h"
#include "sched.h"
#include "terminal.h"
/* We need to declare struct location first :/. */
struct location;
#include "html_r.h"
#include "view.h"

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

/* For struct session */
struct document_setup {
	int assume_cp, hard_assume;
	int use_document_colours;
	int avoid_dark_on_black;
	int tables, frames, images;
	int margin;
	int num_links, table_order;
};

static inline void ds2do(struct document_setup *ds, struct document_options *doo)
{
	doo->assume_cp = ds->assume_cp;
	doo->hard_assume = ds->hard_assume;
	doo->use_document_colours = ds->use_document_colours;
	doo->avoid_dark_on_black = ds->avoid_dark_on_black;
	doo->tables = ds->tables;
	doo->frames = ds->frames;
	doo->images = ds->images;
	doo->margin = ds->margin;
	doo->num_links = ds->num_links;
	doo->table_order = ds->table_order;
}

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
typedef enum {
	WTD_NO,
	WTD_FORWARD,
	WTD_IMGMAP,
	WTD_RELOAD,
	WTD_BACK,
	WTD_UNBACK,
} session_wtd;

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
	session_wtd wtd;
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

time_t parse_http_date(const char *);

unsigned char *encode_url(unsigned char *);
/* unsigned char *decode_url(unsigned char *); */

unsigned char *subst_file(unsigned char *, unsigned char *);

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
void go_back(struct session *);
void go_unback(struct session *);

void abort_loading(struct session *);
void reload(struct session*, int);
void load_frames(struct session *, struct f_data_c *);

struct frame *ses_find_frame(struct session *, unsigned char *);
struct frame *ses_change_frame_url(struct session *, unsigned char *, unsigned char *);

void map_selected(struct terminal *, struct link_def *, struct session *);

/* void destroy_location(struct location *); */
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
