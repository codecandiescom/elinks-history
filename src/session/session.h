/* $Id: session.h,v 1.37 2003/07/04 02:16:35 jonas Exp $ */

#ifndef EL__SCHED_SESSION_H
#define EL__SCHED_SESSION_H

/* We need to declare these first :/. Damn cross-dependencies. */
struct session;

#include "document/cache.h"
#include "document/html/parser.h"
#include "document/html/renderer.h"
#include "document/options.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "sched/connection.h"
#include "sched/location.h"
#include "util/lists.h"
#include "viewer/text/vs.h"


/* This is used to pass along the initial session parameters. */
struct initial_session_info {
	/* The session whose state to copy, -1 is none. */
	int base_session;
	/* The URL we should load immediatelly (or NULL). */
	unsigned char *url;
};

/* This is for map_selected(), it is used to pass around informations about
 * in-imagemap links. */
struct link_def {
	unsigned char *link;
	unsigned char *target;
};

/* This is generic frame descriptor, meaningful mainly for ses_*_frame*(). */
struct frame {
	LIST_HEAD(struct frame);

	unsigned char *name;
	struct view_state vs;

	int redirect_cnt;
};

/* This is the repeat count being inserted by user so far. It is stored
 * intermediately per-session. */
struct kbdprefix {
	int rep;
	int rep_num;
	int prefix;
};

/* This describes, what are we trying to do right now. We pass this around so
 * that we can use generic scheduler routines and when the control will get
 * back to our subsystem, we will know what are we up to. */
enum task_type {
	TASK_NONE,
	TASK_FORWARD,
	TASK_IMGMAP,
	TASK_RELOAD,
	TASK_BACK,
	TASK_UNBACK,
};

/* This is one of the building stones of ELinks architecture --- this structure
 * carries information about the specific ELinks session. Each tab (thus, at
 * least one per terminal, in the normal case) has own session. Session
 * describes mainly the current browsing and control state, from the currently
 * viewed document through the browsing history of this session to the status
 * bar information. */
struct session {
	LIST_HEAD(struct session);


	/* The vital session data */

	int id;

	struct window *tab;


	/* Browsing history */

	struct list_head history; /* -> struct location */
	struct list_head unhistory; /* -> struct location */


	/* The current document */

	/* This points to the current location info. The recommended way of
	 * getting this is by calling cur_loc(session). */
	/* Historical note: this used to be @history.next, but that had to be
	 * changed in order to generalize and greatly simplify the (un)history
	 * handling. --pasky */
	struct location *location;

	struct list_head more_files; /* -> struct file_to_load */

	struct download loading;
	unsigned char *loading_url;

	int reloadlevel;
	int redirect_cnt;

	struct f_data_c *screen;
	struct list_head scrn_frames; /* -> struct f_data_c */

	unsigned char *dn_url;

	unsigned char *ref_url;

	unsigned char *goto_position;

	unsigned char *imgmap_href_base;
	unsigned char *imgmap_target_base;


	/* The current action-in-progress selector */

	enum task_type task;
	unsigned char *task_target;


	/* The current browsing state */

	int search_direction;
	struct kbdprefix kbdprefix;
	int exit_query;
	int display_timer;

	unsigned char *search_word;
	unsigned char *last_search_word;


	/* The possibly running type query (what-to-do-with-that-file?) */

	struct download tq;
	struct cache_entry *tq_ce;
	unsigned char *tq_url;
	unsigned char *tq_goto_position;
	unsigned char *tq_prog;
	int tq_prog_flags;


	/* The Bars */

	int visible_tabs_bar;
	int visible_status_bar;
	int visible_title_bar;

	unsigned char *last_title;
};

extern struct list_head sessions; /* -> struct session */

/* This returns a pointer to the current location inside of the given session.
 * That's nice for encapsulation and alrady paid out once ;-). */
#define cur_loc(x) ((struct location *) ((x)->history.next))

/* Return if we have anything being loaded in this session already. */
static inline int
have_location(struct session *ses) {
	return !list_empty(ses->history);
}


void print_screen_status(struct session *);
void print_error_dialog(struct session *, struct download *);

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
	      enum cache_mode, enum task_type, unsigned char *,
	      void (*)(struct download *, struct session *), int);

void end_load(struct download *, struct session *);
void doc_end_load(struct download *, struct session *);

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
