/* Sessions managment - you'll find things here which you wouldn't expect */
/* $Id: session.c,v 1.105 2003/06/26 21:09:59 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#ifdef USE_LEDS
#include "bfu/leds.h"
#endif
#include "bfu/menu.h"
#include "bfu/msgbox.h"
#include "config/options.h"
#include "dialogs/menu.h"
#include "document/cache.h"
#include "document/options.h"
#include "document/html/parser.h"
#include "document/html/renderer.h"
#include "globhist/globhist.h"
#include "intl/gettext/libintl.h"
#include "lowlevel/home.h"
#include "lowlevel/select.h"
#include "lowlevel/ttime.h"
#include "protocol/protocol.h"
#include "protocol/url.h"
#include "sched/download.h"
#include "sched/error.h"
#include "sched/history.h"
#include "sched/location.h"
#include "sched/sched.h"
#include "sched/session.h"
#include "scripting/lua/hooks.h"
#include "terminal/draw.h"
#include "terminal/tab.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memlist.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/view.h"


struct file_to_load {
	LIST_HEAD(struct file_to_load);

	struct session *ses;
	int req_sent;
	int pri;
	struct cache_entry *ce;
	unsigned char *url;
	struct status stat;
};


INIT_LIST_HEAD(sessions);

static int session_id = 1;


void check_questions_queue(struct session *ses);
struct file_to_load * request_additional_file(struct session *,
					      unsigned char *, int);
struct file_to_load *request_additional_loading_file(struct session *,
						     unsigned char *,
						     struct status *, int);


static unsigned char *
get_stat_msg(struct status *stat, struct terminal *term)
{
	if (stat->state == S_TRANS && stat->prg->elapsed / 100) {
		unsigned char *m = init_str();
		int l = 0;

		add_to_str(&m, &l, _("Received", term));
		add_chr_to_str(&m, &l, ' ');
		add_xnum_to_str(&m, &l, stat->prg->pos + stat->prg->start);
		if (stat->prg->size >= 0) {
			add_chr_to_str(&m, &l, ' ');
			add_to_str(&m, &l, _("of", term));
			add_chr_to_str(&m, &l, ' ');
			add_xnum_to_str(&m, &l, stat->prg->size);
		}
		add_to_str(&m, &l, ", ");
		if (stat->prg->elapsed >= CURRENT_SPD_AFTER * SPD_DISP_TIME) {
			add_to_str(&m, &l, _("avg", term));
			add_chr_to_str(&m, &l, ' ');
		}
		add_xnum_to_str(&m, &l, (longlong)stat->prg->loaded * 10
					/ (stat->prg->elapsed / 100));
		add_to_str(&m, &l, "/s");
		if (stat->prg->elapsed >= CURRENT_SPD_AFTER * SPD_DISP_TIME) {
			add_to_str(&m, &l, ", ");
			add_to_str(&m, &l, _("cur", term));
			add_chr_to_str(&m, &l, ' '),
			add_xnum_to_str(&m, &l, stat->prg->cur_loaded
						/ (CURRENT_SPD_SEC
						   * SPD_DISP_TIME / 1000));
			add_to_str(&m, &l, "/s");
		}

		return m;
	}

	/* debug("%d -> %s", stat->state, _(get_err_msg(stat->state), term)); */
	return stracpy(get_err_msg(stat->state, term));
}

extern struct document_options *d_opt;

void
init_bars_status(struct session *ses, int *tabs_count, struct document_options *doo)
{
	static int prev_title_bar = 0;
	static int prev_status_bar = 0;
	static int prev_tabs_bar = 0;
	int show_title_bar = get_opt_int("ui.show_title_bar");
	int show_status_bar = get_opt_int("ui.show_status_bar");
	int show_tabs_bar = get_opt_int("ui.tabs.show_bar");
	int tabs_cnt = number_of_tabs(ses->tab->term);

	if (!doo && ses->screen && ses->screen->f_data)
		doo = &ses->screen->f_data->opt;

	if (tabs_count) *tabs_count = tabs_cnt;
	ses->visible_tabs_bar = (show_tabs_bar > 0) &&
		 	       !(show_tabs_bar == 1 && tabs_cnt < 2);
	ses->visible_status_bar = show_status_bar;
	ses->visible_title_bar = show_title_bar;

	if (prev_title_bar != ses->visible_title_bar) {
		prev_title_bar = ses->visible_title_bar;
		ses->tab->term->dirty = 1;
	}

	if (prev_status_bar != ses->visible_status_bar) {
		prev_status_bar = ses->visible_status_bar;
		ses->tab->term->dirty = 1;
	}

	if (prev_tabs_bar != ses->visible_tabs_bar) {
		prev_tabs_bar = ses->visible_tabs_bar;
		ses->tab->term->dirty = 1;
	}

	if (doo) {
		doo->xp = 0;
		doo->yp = 0;
		if (ses->visible_title_bar) doo->yp = 1;
		doo->xw = ses->tab->term->x;
		doo->yw = ses->tab->term->y;
		if (ses->visible_title_bar) doo->yw--;
		if (ses->visible_status_bar) doo->yw--;
		if (ses->visible_tabs_bar) doo->yw--;
	}
}

/* Print statusbar and titlebar, set terminal title. */
void
print_screen_status(struct session *ses)
{
	struct terminal *term = ses->tab->term;
	unsigned char *msg = NULL;
	int tabs_count;
	int ses_tab_is_current = (ses->tab == get_current_tab(ses->tab->term));

	init_bars_status(ses, &tabs_count, NULL);

	if (ses->visible_status_bar && ses_tab_is_current) {
		static int last_current_link;
		unsigned int tab_info_len = 0;
		struct status *stat = NULL;

		if (ses->task)
			stat = &ses->loading;
		else if (have_location(ses))
			stat = &cur_loc(ses)->stat;

		if (stat) {
			if (stat->state == S_OK) {
				struct file_to_load *ftl;

				foreach (ftl, ses->more_files) {
					if (ftl->req_sent
					    && ftl->stat.state >= 0) {
						stat = &ftl->stat;
						break;
					}
				}
			}

			/* Show S_INTERRUPTED message *once* but then show links
			 * again as usual. */
			if (current_frame(ses)) {
				int ncl = current_frame(ses)->vs->current_link;

				if (stat->state == S_INTERRUPTED
			    	    && ncl != last_current_link)
					stat->state = S_OK;
				last_current_link = ncl;
			}

			if (stat->state == S_OK)
				msg = print_current_link(ses);
			if (!msg)
				msg = get_stat_msg(stat, term);
		}

		fill_area(term, 0, term->y - 1, term->x, 1,
			  get_bfu_color(term, "status.status-bar"));

		if (!ses->visible_tabs_bar && tabs_count > 1) {
			unsigned char tab_info[8];

			tab_info[tab_info_len++] = '[';
			ulongcat(tab_info, &tab_info_len, term->current_tab + 1, 4, 0);
			tab_info[tab_info_len++] = ']';
			tab_info[tab_info_len++] = ' ';
			tab_info[tab_info_len] = '\0';

			print_text(term, 0, term->y - 1, tab_info_len,
			   	   tab_info,
				   get_bfu_color(term, "status.status-text"));
		}

		if (msg) {
			print_text(term, 0 + tab_info_len, term->y - 1, strlen(msg),
			   	   msg, get_bfu_color(term, "status.status-text"));
			mem_free(msg);
		}
	}

	if (ses->visible_tabs_bar) {
		int tab_width = term->x / tabs_count;
		int tab_num;
		int msglen;
		int normal_color = get_bfu_color(term, "tabs.normal");
		int selected_color = get_bfu_color(term, "tabs.selected");

		for (tab_num = 0; tab_num < tabs_count; tab_num++) {
			struct window *tab = get_tab_by_number(term, tab_num);
			int ypos = term->y - (ses->visible_status_bar ? 2 : 1);
			int color = (tab_num == term->current_tab)
					? selected_color : normal_color;

			if (tab->data
			    && current_frame(tab->data)
			    && current_frame(tab->data)->f_data->title
			    && strlen(current_frame(tab->data)->f_data->title))
				msg = current_frame(tab->data)->f_data->title;
			else
				msg = _("Untitled", term);

			msglen = strlen(msg);
			if (msglen >= tab_width)
				msglen = tab_width - 1;

			fill_area(term, tab_num * tab_width, ypos, tab_width, 1,
				  color);
			print_text(term, tab_num * tab_width, ypos, msglen, msg,
				   color);
			if (tab_width * tabs_count < term->x)
				fill_area(term, (tab_num + 1) * tab_width, ypos,
					  term->x - (tab_width * tabs_count), 1,
				  	  color);

		}
	}

	if (ses->visible_title_bar && ses_tab_is_current) {
		fill_area(term, 0, 0, term->x, 1,
			  get_bfu_color(term, "title.title-bar"));
		msg = print_current_title(ses);
		if (msg) {
			int msglen = strlen(msg);
			int pos = term->x - 1 - msglen;

			if (pos < 0) pos = 0;
			print_text(term, pos, 0, msglen,
				   msg, get_bfu_color(term, "title.title-text"));
			mem_free(msg);
		}
	}

	if (!ses_tab_is_current) goto title_set;
	msg = stracpy("ELinks");
	if (msg) {
		int msglen;
		static void *last_ses = NULL;

		if (ses->screen && ses->screen->f_data
		    && ses->screen->f_data->title
		    && ses->screen->f_data->title[0]) {
			add_to_strn(&msg, " - ");
			add_to_strn(&msg, ses->screen->f_data->title);
		}

		msglen = strlen(msg);
		if ((last_ses != ses ) || !ses->last_title ||
		    strlen(ses->last_title) != msglen ||
		    memcmp(ses->last_title, msg, msglen)) {
			if (ses->last_title) mem_free(ses->last_title);
			ses->last_title = msg;
			set_terminal_title(term, msg);
			last_ses = ses;
		} else {
			mem_free(msg);
		}
	}
title_set:

	redraw_from_window(ses->tab);
#ifdef USE_LEDS
	draw_leds(term);
#endif
}

void
print_error_dialog(struct session *ses, struct status *stat)
{
	unsigned char *t = get_err_msg(stat->state, ses->tab->term);

	if (!t) return;
	msg_box(ses->tab->term, NULL, MSGBOX_NO_INTL,
		_("Error", ses->tab->term), AL_CENTER,
		t,
		ses, 1,
		_("Cancel", ses->tab->term), NULL, B_ENTER | B_ESC /*,
		N_("Retry", ses->tab->term), NULL, 0 */ /* !!! TODO: retry */);
}

static void
free_task(struct session *ses)
{
	if (!ses->task) {
		internal("Session has no task");
		return;
	}

	if (ses->goto_position) {
		mem_free(ses->goto_position);
		ses->goto_position = NULL;
	}

	if (ses->loading_url) {
		mem_free(ses->loading_url);
		ses->loading_url = NULL;
	}
	ses->task = TASK_NONE;
}

static void
abort_files_load(struct session *ses, int interrupt)
{
	struct file_to_load *ftl;
	int q;

	do {
		q = 0;
		foreach (ftl, ses->more_files) {
			if (ftl->stat.state >= 0 && ftl->req_sent) {
				q = 1;
				change_connection(&ftl->stat, NULL, PRI_CANCEL, interrupt);
			}
		}
	} while (q);
}

void
free_files(struct session *ses)
{
	struct file_to_load *ftl;

	abort_files_load(ses, 0);
	foreach (ftl, ses->more_files) {
		if (ftl->ce) ftl->ce->refcount--;
		if (ftl->url) mem_free(ftl->url);
	}
	free_list(ses->more_files);
}


void
ses_forward(struct session *ses)
{
	struct location *l;
	int len;

	free_files(ses);
	if (have_location(ses)) {
		struct frame *frm;

		l = cur_loc(ses);
		foreach (frm, l->frames)
			frm->vs.f = NULL;
		l->vs.f = NULL;
	}

	if (ses->search_word) {
		mem_free(ses->search_word);
	       	ses->search_word = NULL;
	}

x:
	len = strlen(ses->loading_url);
	if (have_location(ses)) {
		int vs_url_len = strlen(cur_loc(ses)->vs.url);

	       	if (len < vs_url_len)
			len = vs_url_len;
	}

	l = mem_alloc(sizeof(struct location) + len + 1);
	if (!l) return;
	memset(l, 0, sizeof(struct location));
	memcpy(&l->stat, &ses->loading, sizeof(struct status));

	if (ses->task_target && *ses->task_target) {
		struct frame *frm;

		if (!have_location(ses)) {
			internal("no location yet");
			return;
		}
		copy_location(l, cur_loc(ses));
		add_to_history(ses, l);
		frm = ses_change_frame_url(ses, ses->task_target,
					   ses->loading_url);

		if (!frm) {
			destroy_location(l);
			ses->task_target = NULL;
			goto x;
		}

		destroy_vs(&frm->vs);
		init_vs(&frm->vs, ses->loading_url);

		if (ses->goto_position) {
			if (frm->vs.goto_position)
				mem_free(frm->vs.goto_position);
			frm->vs.goto_position = ses->goto_position;
			ses->goto_position = NULL;
		}
#if 0
		request_additional_loading_file(ses, ses->loading_url,
						&ses->loading, PRI_FRAME);
#endif
	} else {
		init_list(l->frames);
		init_vs(&l->vs, ses->loading_url);
		add_to_history(ses, l);

		if (ses->goto_position) {
			l->vs.goto_position = ses->goto_position;
			ses->goto_position = NULL;
		}
	}
}

static void
ses_imgmap(struct session *ses)
{
	struct cache_entry *ce;
	struct fragment *fr;
	struct memory_list *ml;
	struct menu_item *menu;

	if (!find_in_cache(ses->loading_url, &ce) || !ce) {
		internal("can't find cache entry");
		return;
	}
	defrag_entry(ce);
	fr = ce->frag.next;
	if ((void *)fr == &ce->frag) return;

	if (get_image_map(ce->head, fr->data, fr->data + fr->length,
			  ses->goto_position, &menu, &ml,
			  ses->imgmap_href_base, ses->imgmap_target_base,
			  get_opt_int_tree(ses->tab->term->spec, "charset"),
			  get_opt_int("document.codepage.assume"),
			  get_opt_int("document.codepage.force_assumed")))
		return;

	add_empty_window(ses->tab->term, (void (*)(void *))freeml, ml);
	do_menu(ses->tab->term, menu, ses, 0);
}

void
map_selected(struct terminal *term, struct link_def *ld, struct session *ses)
{
	goto_url_frame(ses, ld->link, ld->target);
}


void file_end_load(struct status *, struct file_to_load *);
void abort_preloading(struct session *, int);

struct task {
	struct session *ses;
	unsigned char *url;
	int pri;
	enum cache_mode cache_mode;
	enum task_type type;
	unsigned char *target;
	unsigned char *pos;
	void (*fn)(struct status *, struct session *);
};


static void
post_yes(struct task *task)
{
	struct session *ses = task->ses;

	abort_preloading(task->ses, 0);
	if (task->ses->goto_position) mem_free(task->ses->goto_position);

	ses->goto_position = task->pos ? stracpy(task->pos) : NULL;
	ses->loading.end = (void (*)(struct status *, void *))task->fn;
	ses->loading.data = task->ses;
	ses->loading_url = stracpy(task->url);
	ses->task = task->type;
	ses->task_target = task->target;

	load_url(ses->loading_url, ses->ref_url,
		 &ses->loading, task->pri, task->cache_mode, -1);
}

static void
post_no(struct task *task)
{
	reload(task->ses, NC_CACHE);
}

void
ses_goto(struct session *ses, unsigned char *url, unsigned char *target,
	 int pri, enum cache_mode cache_mode, enum task_type task_type,
	 unsigned char *pos,
	 void (*fn)(struct status *, struct session *),
	 int redir)
{
	struct task *task = mem_alloc(sizeof(struct task));
	unsigned char *m1, *m2;
	struct cache_entry *e;
	unsigned char *post_char_pos = strchr(url, POST_CHAR);

	if (!task || !get_opt_int("document.browse.forms.confirm_submit")
	    || !post_char_pos
	    || (cache_mode == NC_ALWAYS_CACHE && find_in_cache(url, &e)
		&& !e->incomplete)) {

		if (task) mem_free(task);

		if (ses->goto_position) mem_free(ses->goto_position);
		ses->goto_position = pos;

		ses->loading.end = (void (*)(struct status *, void *))fn;
		ses->loading.data = ses;
		ses->loading_url = url;
		ses->task = task_type;
		ses->task_target = target;

		load_url(url, ses->ref_url, &ses->loading, pri, cache_mode, -1);

		return;
	}

	task->ses = ses;
	task->url = url;
	task->pri = pri;
	task->cache_mode = cache_mode;
	task->type = task_type;
	task->target = target;
	task->pos = pos;
	task->fn = fn;

	if (redir) {
		m1 = N_("Do you want to follow redirect and post form data "
			"to url %s?");
	} else if (task_type == TASK_FORWARD) {
		m1 = N_("Do you want to post form data to url %s?");
	} else {
		m1 = N_("Do you want to repost form data to url %s?");
	}

	m2 = memacpy(url, post_char_pos - url);
	msg_box(ses->tab->term, getml(m2, task, task->url, task->pos,
				 NULL), MSGBOX_FREE_TEXT,
		N_("Warning"), AL_CENTER,
		msg_text(ses->tab->term, m1, m2),
		task, 2,
		N_("Yes"), post_yes, B_ENTER,
		N_("No"), post_no, B_ESC);
}

static int
do_move(struct session *ses, struct status **stat)
{
	struct cache_entry *ce = NULL;

	if (!ses->loading_url) {
		internal("no ses->loading_url");
		return 0;
	}

	ce = (*stat)->ce;
	if (!ce || (ses->task == TASK_IMGMAP && (*stat)->state >= 0)) {
		return 0;
	}

	if (ce->redirect && ses->redirect_cnt++ < MAX_REDIRECTS) {
		unsigned char *u, *p;
		enum task_type task = ses->task;

		if (task == TASK_BACK && !have_location(ses))
			goto b;

		u = join_urls(ses->loading_url, ce->redirect);
		if (!u) goto b;

		if (!get_opt_int("protocol.http.bugs.broken_302_redirect")) {
			if (!ce->redirect_get) {
				p = strchr(ses->loading_url, POST_CHAR);
				if (p) add_to_strn(&u, p);
			}
		}
		/* ^^^^ According to RFC2068 POST must not be redirected to GET, but
			some BUGGY message boards rely on it :-( */

		abort_loading(ses, 0);
		if (have_location(ses))
			*stat = &cur_loc(ses)->stat;
		else
			*stat = NULL;

		if (ses->ref_url)
			mem_free(ses->ref_url);
		ses->ref_url = stracpy(ce->url);

		if (task == TASK_FORWARD || task == TASK_IMGMAP) {
			unsigned char *gp = ses->goto_position ?
					    stracpy(ses->goto_position) : NULL;
			ses_goto(ses, u, ses->task_target, PRI_MAIN, NC_CACHE,
				 task, gp, end_load, 1);

			if (gp) mem_free(gp);

			return 2;
		}

		if (task == TASK_BACK || task == TASK_UNBACK) {
			ses_goto(ses, u, NULL, PRI_MAIN, NC_CACHE,
				 TASK_RELOAD, NULL, end_load, 1);
			return 2;
		}
		if (task == TASK_RELOAD) {
			ses_goto(ses, u, NULL, PRI_MAIN, ses->reloadlevel,
				 TASK_RELOAD, NULL, end_load, 1);
			return 2;
		}
	} else {
		ses->redirect_cnt = 0;
	}

b:
	if (ses->display_timer != -1) {
		kill_timer(ses->display_timer);
	       	ses->display_timer = -1;
	}
	if (ses->task == TASK_FORWARD) {
		if (ses_chktype(ses, stat, ce)) {
			free_task(ses);
			reload(ses, NC_CACHE);
			return 2;
		}
	}
	if (ses->task == TASK_IMGMAP) ses_imgmap(ses);
	if (ses->task == TASK_BACK) ses_back(ses);
	if (ses->task == TASK_UNBACK) ses_unback(ses);
	if (ses->task == TASK_RELOAD) ses_back(ses), ses_forward(ses);
	if ((*stat)->state >= 0) {
		*stat = &cur_loc(ses)->stat;
		change_connection(&ses->loading, *stat, PRI_MAIN, 0);
	} else {
		cur_loc(ses)->stat.state = ses->loading.state;
	}

	free_task(ses);
	return 1;
}


void request_frameset(struct session *, struct frameset_desc *);

static void
request_frame(struct session *ses, unsigned char *name, unsigned char *uurl)
{
	struct location *loc = cur_loc(ses);
	struct frame *frm;
	unsigned char *url, *pos;

	if (!have_location(ses)) {
		internal("request_frame: no location");
		return;
	}

	foreach (frm, loc->frames) {
		if (strcasecmp(frm->name, name))
			continue;

		url = stracpy(frm->vs.url);
		if (!url
		    || (frm->vs.f && frm->vs.f->f_data
		    && frm->vs.f->f_data->frame)) {
			/* del_from_list(frm); */
			request_frameset(ses, frm->vs.f->f_data->frame_desc);
#if 0
			destroy_vs(&frm->vs);
			mem_free(frm->name);
			mem_free(frm);
#endif
			if (url) mem_free(url);
			return;
		}
		goto found;
	}

	url = stracpy(uurl);
	if (!url) return;
	pos = extract_position(url);

	frm = mem_alloc(sizeof(struct frame) + strlen(url) + 1);
	if (!frm) {
		mem_free(url);
		if (pos) mem_free(pos);
		return;
	}
	memset(frm, 0, sizeof(struct frame));

	frm->name = stracpy(name);
	if (!frm->name) {
		mem_free(frm);
		mem_free(url);
		if (pos) mem_free(pos);
		return;
	}

	init_vs(&frm->vs, url);
	if (pos) frm->vs.goto_position = pos;

	add_to_list(loc->frames, frm);

found:
	if (url) {
		if (*url) {
			request_additional_file(ses, url, PRI_FRAME);
		}

		mem_free(url);
	}
}

void
request_frameset(struct session *ses, struct frameset_desc *fd)
{
	static int depth = 0; /* Inheritation counter (recursion brake ;) */
	int i;

	if (++depth <= HTML_MAX_FRAME_DEPTH) {
		for (i = 0; i < fd->n; i++) {
			if (fd->f[i].subframe) {
				request_frameset(ses, fd->f[i].subframe);
			} else if (fd->f[i].name) {
				request_frame(ses, fd->f[i].name,
					      fd->f[i].url);
			}
		}
	}

	depth--;
}

inline void
load_frames(struct session *ses, struct f_data_c *fd)
{
	struct f_data *ff = fd->f_data;

	if (!ff || !ff->frame) return;
	request_frameset(ses, ff->frame_desc);
}

void
display_timer(struct session *ses)
{
	ttime t = get_time();
	html_interpret(ses);
	draw_formatted(ses);

	t = (get_time() - t) * DISPLAY_TIME;
	if (t < DISPLAY_TIME_MIN) t = DISPLAY_TIME_MIN;

	ses->display_timer = install_timer(t, (void (*)(void *))display_timer,
					   ses);
	load_frames(ses, ses->screen);
	process_file_requests(ses);
}


struct questions_entry {
	LIST_HEAD(struct questions_entry);

	void (*callback)(struct session *);
};

INIT_LIST_HEAD(questions_queue);


void
check_questions_queue(struct session *ses)
{
	struct questions_entry *q;

	while (!list_empty(questions_queue)) {
		q = questions_queue.next;
		q->callback(ses);
		del_from_list(q);
		mem_free(q);
	}
}

void
add_questions_entry(void *callback)
{
	struct questions_entry *q = mem_alloc(sizeof(struct questions_entry));

	if (!q) return;
	q->callback = callback;
	add_to_list(questions_queue, q);
}

void
end_load(struct status *stat, struct session *ses)
{
	int d;

	if (!ses->task) {
		internal("end_load: no ses->task");
		return;
	}
	d = do_move(ses, &stat);
	if (!stat) return;
	if (d == 1) {
		stat->end = (void (*)(struct status *, void *))doc_end_load;
		display_timer(ses);
	}
	if (stat->state < 0) {
		if (d != 2 && ses->task) {
			free_task(ses);
		}
		if (d == 1) doc_end_load(stat, ses);
	}
	if (stat->state < 0 && stat->state != S_OK && d != 2) {
		print_error_dialog(ses, stat);
		if (!d) reload(ses, NC_CACHE);
	}
	check_questions_queue(ses);
	print_screen_status(ses);
}

#ifdef HAVE_SCRIPTING
static void
maybe_pre_format_html(struct cache_entry *ce, struct session *ses)
{
	struct fragment *fr;
	unsigned char *s;
	int len;

	if (ce && !ce->done_pre_format_html_hook) {
		defrag_entry(ce);
		fr = ce->frag.next;
		len = fr->length;
		s = script_hook_pre_format_html(ses, ce->url, fr->data, &len);
		if (s) {
			add_fragment(ce, 0, s, len);
			truncate_entry(ce, len, 1);
			ce->incomplete = 0; /* XXX */
			mem_free(s);
		}
		ce->done_pre_format_html_hook = 1;
	}
}
#endif

unsigned char *get_form_url(struct session *, struct f_data_c *,
				    struct form_control *);
int goto_link(unsigned char *, unsigned char *, struct session *, int);

void
doc_end_load(struct status *stat, struct session *ses)
{
	int submit = 0;
	struct form_control *fc = NULL;

	if (stat->state < 0) {
#ifdef HAVE_SCRIPTING
		maybe_pre_format_html(stat->ce, ses);
#endif
		if (ses->display_timer != -1) {
			kill_timer(ses->display_timer);
			ses->display_timer = -1;
		}
		html_interpret(ses);
		draw_formatted(ses);
		if (get_opt_bool_tree(&cmdline_options, "auto-submit")) {
			fc = (struct form_control *)
				ses->screen->f_data->forms.next;
			if (fc != fc->next) {
				get_opt_bool_tree(&cmdline_options,
						  "auto-submit") = 0;
				submit = 1;
			}
		}
		load_frames(ses, ses->screen);
		process_file_requests(ses);
		if (stat->state != S_OK)
			print_error_dialog(ses, stat);

	} else if (ses->display_timer == -1) display_timer(ses);

	check_questions_queue(ses);
	print_screen_status(ses);

#ifdef GLOBHIST
	add_global_history_item(cur_loc(ses)->vs.url,
				ses->screen->f_data->title, time(NULL));
#endif

	if (submit) {
		goto_link(get_form_url(ses, ses->screen, fc), fc->target, ses,
			  1);
	}
}

void
file_end_load(struct status *stat, struct file_to_load *ftl)
{
	if (ftl->stat.ce) {
		if (ftl->ce) ftl->ce->refcount--;
		ftl->ce = ftl->stat.ce;
		ftl->ce->refcount++;
	}

	/* FIXME: We need to do content-type check here! However, we won't
	 * handle properly the "Choose action" dialog now :(. */
#if 0
	if (ses_chktype(ftl->ses, stat, ftl->ce)) {
#if 0
		free_wtd(ftl->ses);
		reload(ses, NC_CACHE);
#endif
		return;
	}
#endif

	doc_end_load(stat, ftl->ses);
}

struct file_to_load *
request_additional_file(struct session *ses, unsigned char *url, int pri)
{
	struct file_to_load *ftl;

	foreach (ftl, ses->more_files) {
		if (!strcmp(ftl->url, url)) {
			if (ftl->pri > pri) {
				ftl->pri = pri;
				change_connection(&ftl->stat, &ftl->stat, pri, 0);
			}
			return NULL;
		}
	}

	ftl = mem_alloc(sizeof(struct file_to_load));
	if (!ftl) {
		return NULL;
	}

	ftl->url = stracpy(url);
	if (!ftl->url) {
		mem_free(ftl);
		return NULL;
	}

	ftl->stat.end = (void (*)(struct status *, void *)) file_end_load;
	ftl->stat.data = ftl;
	ftl->req_sent = 0;
	ftl->pri = pri;
	ftl->ce = NULL;
	ftl->ses = ses;

	add_to_list(ses->more_files, ftl);

	return ftl;
}

struct file_to_load *
request_additional_loading_file(struct session *ses, unsigned char *url,
				struct status *stat, int pri)
{
	struct file_to_load *ftl;

	ftl = request_additional_file(ses, url, pri);
	if (!ftl) {
		change_connection(stat, NULL, PRI_CANCEL, 0);
		return NULL;
	}

	ftl->req_sent = 1;
	ftl->ce = stat->ce;

	change_connection(stat, &ftl->stat, pri, 0);

	return ftl;
}

void
process_file_requests(struct session *ses)
{
	static int stop_recursion = 0;
	struct file_to_load *ftl;
	struct f_data_c *fd = current_frame(ses);
	int more = 1;

	if (stop_recursion) return;
	stop_recursion = 1;

	while (more) {
		more = 0;
		foreach (ftl, ses->more_files) {
			if (ftl->req_sent)
				continue;

			ftl->req_sent = 1;
			load_url(ftl->url, (fd && fd->f_data) ? fd->f_data->url
							      : NULL,
				 &ftl->stat, ftl->pri, NC_CACHE, -1);
			more = 1;
		}
	}

	stop_recursion = 0;
}

struct session *
create_basic_session(struct window *tab)
{
        struct session *ses = mem_calloc(1, sizeof(struct session));

	if (!ses) return NULL;

	create_history(ses);
	init_list(ses->scrn_frames);
	init_list(ses->more_files);
	ses->tab = tab;
	ses->id = session_id++;
	ses->screen = NULL;
	ses->task = TASK_NONE;
	ses->display_timer = -1;
	ses->loading_url = NULL;
	ses->goto_position = NULL;
	ses->last_title = NULL;

	add_to_list(sessions, ses);

	return ses;
}

static struct session *
create_session(struct window *tab)
{
	struct terminal *term = tab->term;
	struct session *ses = create_basic_session(tab);

	if (!ses) return NULL;

	if (first_use) {
		first_use = 0;
		msg_box(term, NULL, 0,
			N_("Welcome"), AL_CENTER,
			N_("Welcome to ELinks!\n\n"
			"Press ESC for menu. Documentation is available in "
			"Help menu."),
			NULL, 1,
			N_("OK"), NULL, B_ENTER | B_ESC);
	}

	if (!*get_opt_str("protocol.http.user_agent")) {
		msg_box(term, NULL, 0,
			N_("Warning"), AL_CENTER,
			N_("You have empty string in protocol.http.user_agent - "
			"this was a default value in the past, substituted by "
			"default ELinks User-Agent string. However, currently "
			"this means that NO User-Agent HEADER "
			"WILL BE SENT AT ALL - if this is really what you want, "
			"set its value to \" \", otherwise please delete line "
			"with this settings from your configuration file (if you "
			"have no idea what I'm talking about, just do this), so "
			"that correct default setting will be used. Apologies for "
			"any inconvience caused."),
			NULL, 1,
			N_("OK"), NULL, B_ENTER | B_ESC);
	}

	if (!get_opt_bool("config.saving_style_w")) {
		get_opt_bool("config.saving_style_w") = 1;
		get_opt_rec(&root_options, "config.saving_style_w")->flags |= OPT_TOUCHED;
		if (get_opt_int("config.saving_style") != 3) {
			msg_box(term, NULL, 0,
				N_("Warning"), AL_CENTER,
				N_("You have option config.saving_style set to "
				"a de facto obsolete value. The configuration "
				"saving algorithms of ELinks were changed from "
				"the last time you upgraded ELinks. Now, only "
				"those options which you actually changed are "
				"saved to the configuration file, instead of "
				"just all the options. This simplifies our "
				"situation greatly when we see that some option "
				"has inappropriate default value or we need to "
				"change semantic of some option in a subtle way. "
				"Thus, we recommend you to change the value of "
				"config.saving_style option to 3 in order to get "
				"the \"right\" behaviour. Apologies for any "
				"inconvience caused."),
				NULL, 1,
				N_("OK"), NULL, B_ENTER | B_ESC);
		}
	}

	return ses;
}

static inline void
copy_session(struct session *old, struct session *new)
{
	if (have_location(old)) {
		goto_url(new, cur_loc(old)->vs.url);
	}
}

void *
create_session_info(int cp, unsigned char *url, int *ll)
{
	int l = strlen(url);
	int *i;

	*ll = 2 * sizeof(int) + l;

	i = mem_alloc(*ll);
	if (!i) return NULL;

	i[0] = cp;
	i[1] = l;
	memcpy(i + 2, url, l);

	return i;
}


struct initial_session_info *
decode_session_info(const void *pdata)
{
	int *data = (int *) pdata;
	int len = *(data++);
	struct initial_session_info *info;
	int url_len;

	if (len < 2 * sizeof(int)) return NULL;

	info = mem_calloc(1, sizeof(struct initial_session_info));
	if (!info) return NULL;

	info->base_session = *(data++);

	url_len = *(data++);
	if (url_len) {
		unsigned char *url;

		if (len < 2 * sizeof(int) + url_len) goto url_decoded;

		url = mem_alloc(url_len + 1);
		if (!url) goto url_decoded;

		memcpy(url, data, url_len);
		url[url_len] = '\0';

		info->url = decode_shell_safe_url(url);

		mem_free(url);
	}
url_decoded:

	return info;
}

static void
free_session_info(struct initial_session_info *info)
{
	if (info->url) mem_free(info->url);
	mem_free(info);
}

static void
dialog_goto_url_open(void *data)
{
	struct session *ses = data;

	dialog_goto_url(ses, NULL);
}

static int
process_session_info(struct session *ses, struct initial_session_info *info)
{
	struct session *s;

	if (!info) return -1;

	/* This is the only place where s->id comes into game - we're comparing
	 * it to possibly supplied -base-session here, and clone the session
	 * with id of base-session (its current document association only,
	 * rather) to the newly created session. */
	foreach (s, sessions) {
		if (s->id == info->base_session) {
			copy_session(s, ses);
			break;
		}
	}

	if (info->url) {
		goto_url(ses, info->url);
	} else {
		unsigned char *h = getenv("WWW_HOME");

		if (!h || !*h)
			h = WWW_HOME_URL;
		if (!h || !*h) {
			if (get_opt_int("ui.startup_goto_dialog")) {
				/* We can't create new window in EV_INIT
				 * handler! */
				register_bottom_half(dialog_goto_url_open, ses);
			}
		} else {
			goto_url(ses, h);
		}
	}

	free_session_info(info);
	return 0;
}

void
abort_preloading(struct session *ses, int interrupt)
{
	if (ses->task) {
		change_connection(&ses->loading, NULL, PRI_CANCEL, interrupt);
		free_task(ses);
	}
}

void
abort_loading(struct session *ses, int interrupt)
{
	if (have_location(ses)) {
		struct location *l = cur_loc(ses);

		if (l->stat.state >= 0)
			change_connection(&l->stat, NULL, PRI_CANCEL, interrupt);
		abort_files_load(ses, interrupt);
	}
	abort_preloading(ses, interrupt);
}

static void
destroy_session(struct session *ses)
{
	struct f_data_c *fdc;

	if (!ses) return;

	destroy_downloads(ses);
	abort_loading(ses, 0);
	free_files(ses);
	if (ses->screen) {
		detach_formatted(ses->screen);
		mem_free(ses->screen);
	}

	foreach (fdc, ses->scrn_frames)
		detach_formatted(fdc);

	free_list(ses->scrn_frames);

	destroy_history(ses);
	if (ses->location) destroy_location(ses->location);

	if (ses->loading_url) mem_free(ses->loading_url);
	if (ses->display_timer != -1) kill_timer(ses->display_timer);
	if (ses->goto_position) mem_free(ses->goto_position);
	if (ses->imgmap_href_base) mem_free(ses->imgmap_href_base);
	if (ses->imgmap_target_base) mem_free(ses->imgmap_target_base);
	if (ses->tq_ce) ses->tq_ce->refcount--;
	if (ses->tq_url) {
		change_connection(&ses->tq, NULL, PRI_CANCEL, 0);
		mem_free(ses->tq_url);
	}
	if (ses->tq_goto_position) mem_free(ses->tq_goto_position);
	if (ses->tq_prog) mem_free(ses->tq_prog);
	if (ses->dn_url) mem_free(ses->dn_url);
	if (ses->ref_url) mem_free(ses->ref_url), ses->ref_url = NULL;
	if (ses->search_word) mem_free(ses->search_word);
	if (ses->last_search_word) mem_free(ses->last_search_word);
	if (ses->last_title) mem_free(ses->last_title);
	del_from_list(ses);
	/*mem_free(ses);*/
}

void
destroy_all_sessions(void)
{
	/*while (!list_empty(sessions)) destroy_session(sessions.next);*/
}

void
reload(struct session *ses, enum cache_mode cache_mode)
{
	abort_loading(ses, 0);

	if (cache_mode == -1)
		cache_mode = ++ses->reloadlevel;
	else
		ses->reloadlevel = cache_mode;

	if (have_location(ses)) {
		struct location *l = cur_loc(ses);
		struct file_to_load *ftl;
		struct f_data_c *fd = current_frame(ses);

		l->stat.data = ses;
		l->stat.end = (void *)doc_end_load;
		load_url(l->vs.url, ses->ref_url, &l->stat, PRI_MAIN, cache_mode, -1);
		foreach (ftl, ses->more_files) {
			if (ftl->req_sent && ftl->stat.state >= 0) continue;
			ftl->stat.data = ftl;
			ftl->stat.end = (void *)file_end_load;
			load_url(ftl->url, fd?fd->f_data?fd->f_data->url:NULL:NULL,
				 &ftl->stat, PRI_FRAME, cache_mode, -1);
		}
	}
}

#if 0
void
ses_load_notify(struct status *stat, struct session *ses)
{
	if (stat->state == S_TRANS || stat->state == S_OK) {
		stat->end = (void (*)(struct status *, void *))end_load;
		ses->wtd = WTD_NO;
		mem_free(ses->loading_url);
		if (ses->wtd == WTD_FORWARD) {
			ses_forward(ses);
		} else internal("bad ses->wtd");
		return;
	}
	if (stat->state >= 0) print_screen_status(ses);
	if (stat->state < 0) print_error_dlg(ses, stat);
}
#endif

static void
really_goto_url_w(struct session *ses, unsigned char *url, unsigned char *target,
		  enum task_type task, enum cache_mode cache_mode)
{
	unsigned char *u;
	unsigned char *pos;
	void (*fn)(struct session *, unsigned char *);
	struct f_data_c *fd;

	fn = get_protocol_external_handler(url);
	if (fn) {
		fn(ses, url);
		goto end;
	}

	ses->reloadlevel = cache_mode;

	u = translate_url(url, ses->tab->term->cwd);
	if (!u) {
		struct status stat = { NULL_LIST_HEAD, NULL, NULL,
				       NULL, NULL, NULL,
				       S_BAD_URL, PRI_CANCEL, 0 };

		print_error_dialog(ses, &stat);
		goto end;
	}

	pos = extract_position(u);

	if (ses->task == task) {
		if (!strcmp(ses->loading_url, u)) {
			/* We're already loading the URL. */
			mem_free(u);

			if (ses->goto_position)
				mem_free(ses->goto_position);
			ses->goto_position = pos;

			goto end;
		}
	}

	abort_loading(ses, 0);
	if (ses->ref_url) {
		mem_free(ses->ref_url);
		ses->ref_url = NULL;
	}

	fd = current_frame(ses);
	if (fd && fd->f_data && fd->f_data->url)
 		ses->ref_url = stracpy(fd->f_data->url);

	ses_goto(ses, u, target, PRI_MAIN, cache_mode, task, pos, end_load, 0);

	/* abort_loading(ses); */

end:
	clean_unhistory(ses);
}

static void
goto_url_w(struct session *ses, unsigned char *url, unsigned char *target,
	   enum task_type task, enum cache_mode cache_mode)
{
#ifdef HAVE_SCRIPTING
	url = script_hook_follow_url(ses, url);
	if (url) {
		really_goto_url_w(ses, url, target, task, cache_mode);
		mem_free(url);
	}
#else
	really_goto_url_w(ses, url, target, task, cache_mode);
#endif
}

void
goto_url_frame_reload(struct session *ses, unsigned char *url,
		      unsigned char *target)
{
	goto_url_w(ses, url, target, TASK_FORWARD, NC_RELOAD);
}

void
goto_url_frame(struct session *ses, unsigned char *url,
	       unsigned char *target)
{
	goto_url_w(ses, url, target, TASK_FORWARD, NC_CACHE);
}

void
goto_url(struct session *ses, unsigned char *url)
{
	goto_url_w(ses, url, NULL, TASK_FORWARD, NC_CACHE);
}

void
goto_url_with_hook(struct session *ses, unsigned char *url)
{
#ifdef HAVE_SCRIPTING
	script_hook_goto_url(ses, url);
#else
	goto_url(ses, url);
#endif
}

/* TODO: Should there be goto_imgmap_reload() ? */

void
goto_imgmap(struct session *ses, unsigned char *url, unsigned char *href,
	    unsigned char *target)
{
	if (ses->imgmap_href_base) mem_free(ses->imgmap_href_base);
	ses->imgmap_href_base = href;
	if (ses->imgmap_target_base) mem_free(ses->imgmap_target_base);
	ses->imgmap_target_base = target;
	goto_url_w(ses, url, target, TASK_IMGMAP, NC_CACHE);
}

struct frame *
ses_find_frame(struct session *ses, unsigned char *name)
{
	struct location *l = cur_loc(ses);
	struct frame *frm;

	if (!have_location(ses)) {
		internal("ses_request_frame: no location yet");
		return NULL;
	}

	foreachback (frm, l->frames)
		if (!strcasecmp(frm->name, name))
			return frm;

	return NULL;
}

struct frame *
ses_change_frame_url(struct session *ses, unsigned char *name,
		     unsigned char *url)
{
	struct location *l = cur_loc(ses);
	struct frame *frm;
	size_t url_len = strlen(url);

	if (!have_location(ses)) {
		internal("ses_change_frame_url: no location yet");
		return NULL;
	}

	foreachback (frm, l->frames) {
		if (strcasecmp(frm->name, name)) continue;

		if (url_len > strlen(frm->vs.url)) {
			struct f_data_c *fd;
			struct frame *nf = frm;

			nf = mem_realloc(frm, sizeof(struct frame)
				      + url_len + 1);
			if (!nf) return NULL;

			nf->prev->next = nf->next->prev = nf;

			foreach (fd, ses->scrn_frames)
				if (fd->vs == &frm->vs)
					fd->vs = &nf->vs;

			frm = nf;
		}
		memcpy(frm->vs.url, url, url_len + 1);

		return frm;
	}

	return NULL;

}

void
tabwin_func(struct window *tab, struct event *ev, int fw)
{
	struct session *ses = tab->data;

	switch (ev->ev) {
		case EV_ABORT:
			if (ses) destroy_session(ses);
			break;
		case EV_INIT:
			/* Perhaps we should call just create_base_session()
			 * and then do the rest of create_session() stuff
			 * (renamed to setup_first_session() or so) if this is
			 * the first tab. But I don't think it is so urgent.
			 * --pasky */
			ses = tab->data = create_session(tab);
			if (!ses || process_session_info(ses, (struct initial_session_info *) ev->b)) {
				destroy_terminal(tab->term);
				return;
			}
			/* fall-through */
		case EV_RESIZE:
			if (!ses) break;
			html_interpret(ses);
			draw_formatted(ses);
			load_frames(ses, ses->screen);
			process_file_requests(ses);
			print_screen_status(ses);
			break;
		case EV_REDRAW:
			if (!ses) break;
			draw_formatted(ses);
			print_screen_status(ses);
			break;
		case EV_KBD:
		case EV_MOUSE:
			if (!ses) break;
			send_event(ses, ev);
			break;
		default:
			error(_("Unknown event.", tab->term));
	}
}

/*
 * Gets the url being viewed by this session. Writes it into str.
 * A maximum of str_size bytes (including null) will be written.
 */
unsigned char *
get_current_url(struct session *ses, unsigned char *str, size_t str_size)
{
	unsigned char *here, *end_of_url;
	size_t url_len = 0;

	/* Not looking at anything */
	if (!have_location(ses))
		return NULL;

	here = cur_loc(ses)->vs.url;

	/* Find the length of the url */
	end_of_url = strchr(here, POST_CHAR);
	if (end_of_url) {
		url_len = (size_t) (end_of_url - here);
	} else {
		url_len = strlen(here);
	}

	/* Ensure that the url size is not greater than
	 * str_size. We can't just happily
	 * strncpy(str, here, str_size)
	 * because we have to stop at POST_CHAR, not only at
	 * NULL. */
	if (url_len >= str_size)
			url_len = str_size - 1;

	return safe_strncpy(str, here, url_len + 1);
}


/*
 * Gets the title of the page being viewed by this session. Writes it into str.
 * A maximum of str_size bytes (including null) will be written.
 */
unsigned char *
get_current_title(struct session *ses, unsigned char *str, size_t str_size)
{
	struct f_data_c *fd = current_frame(ses);

	/* Ensure that the title is defined */
	if (fd && fd->f_data->title)
		return safe_strncpy(str, fd->f_data->title, str_size);

	return NULL;
}

/*
 * Gets the url of the link currently selected. Writes it into str.
 * A maximum of str_size bytes (including null) will be written.
 */
unsigned char *
get_current_link_url(struct session *ses, unsigned char *str, size_t str_size)
{
	struct link *l = get_current_link(ses);

	if (l) return safe_strncpy(str, l->where ? l->where : l->where_img,
				   str_size);

	return NULL;
}

/* get_current_link_name: returns the name of the current link
 * (the text between <A> and </A>), str is a preallocated string,
 * str_size includes the null char. */
unsigned char *
get_current_link_name(struct session *ses, unsigned char *str, size_t str_size)
{
	struct link *l = get_current_link(ses);

	if (l) return safe_strncpy(str, l->name, str_size);

	return NULL;
}

struct link *
get_current_link(struct session *ses)
{
	struct f_data_c *fd = current_frame(ses); /* What the hell is an 'fd'? */

	if (fd && fd->vs->current_link != -1) {
		struct link *l = &fd->f_data->links[fd->vs->current_link];

		/* Only return a link */
		if (l->type == L_LINK) return l;
	}

	return NULL;
}
