/* Sessions managment - you'll find things here which you wouldn't expect */
/* $Id: session.c,v 1.228 2003/11/14 02:06:29 miciah Exp $ */

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
#include "bfu/style.h"
#include "config/options.h"
#include "dialogs/menu.h"
#include "cache/cache.h"
#include "document/options.h"
#include "document/html/parser.h"
#include "document/html/renderer.h"
#include "globhist/globhist.h"
#include "intl/gettext/libintl.h"
#include "lowlevel/home.h"
#include "lowlevel/select.h"
#include "lowlevel/ttime.h"
#include "protocol/protocol.h"
#include "protocol/uri.h"
#include "sched/connection.h"
#include "sched/download.h"
#include "sched/error.h"
#include "sched/event.h"
#include "sched/history.h"
#include "sched/location.h"
#include "sched/session.h"
#include "terminal/draw.h"
#include "terminal/screen.h"
#include "terminal/tab.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/color.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memlist.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/form.h"
#include "viewer/text/link.h"
#include "viewer/text/view.h"


struct file_to_load {
	LIST_HEAD(struct file_to_load);

	struct session *ses;
	int req_sent;
	int pri;
	struct cache_entry *ce;
	unsigned char *url;
	struct download stat;
};


INIT_LIST_HEAD(sessions);

static int session_id = 1;


void check_questions_queue(struct session *ses);
struct file_to_load * request_additional_file(struct session *,
					      unsigned char *, int);
struct file_to_load *request_additional_loading_file(struct session *,
						     unsigned char *,
						     struct download *, int);


static unsigned char *
get_stat_msg(struct download *stat, struct terminal *term)
{
	if (stat->state == S_TRANS && stat->prg->elapsed / 100) {
		struct string msg;

		if (!init_string(&msg)) return NULL;

		add_to_string(&msg, _("Received", term));
		add_char_to_string(&msg, ' ');
		add_xnum_to_string(&msg, stat->prg->pos + stat->prg->start);
		if (stat->prg->size >= 0) {
			add_char_to_string(&msg, ' ');
			add_to_string(&msg, _("of", term));
			add_char_to_string(&msg, ' ');
			add_xnum_to_string(&msg, stat->prg->size);
		}
		add_to_string(&msg, ", ");
		if (stat->prg->elapsed >= CURRENT_SPD_AFTER * SPD_DISP_TIME) {
			add_to_string(&msg, _("avg", term));
			add_char_to_string(&msg, ' ');
		}
		add_xnum_to_string(&msg, (longlong)stat->prg->loaded * 10
					 / (stat->prg->elapsed / 100));
		add_to_string(&msg, "/s");
		if (stat->prg->elapsed >= CURRENT_SPD_AFTER * SPD_DISP_TIME) {
			add_to_string(&msg, ", ");
			add_to_string(&msg, _("cur", term));
			add_char_to_string(&msg, ' '),
			add_xnum_to_string(&msg, stat->prg->cur_loaded
						 / (CURRENT_SPD_SEC
						 * SPD_DISP_TIME / 1000));
			add_to_string(&msg, "/s");
		}

		return msg.source;
	}

	/* debug("%d -> %s", stat->state, _(get_err_msg(stat->state), term)); */
	return stracpy(get_err_msg(stat->state, term));
}

extern struct document_options *global_doc_opts;

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

	if (!doo && ses->doc_view && ses->doc_view->document)
		doo = &ses->doc_view->document->options;

	if (tabs_count) *tabs_count = tabs_cnt;
	ses->visible_tabs_bar = (show_tabs_bar > 0) &&
				!(show_tabs_bar == 1 && tabs_cnt < 2);
	ses->visible_status_bar = show_status_bar;
	ses->visible_title_bar = show_title_bar;

	if (prev_title_bar != ses->visible_title_bar) {
		prev_title_bar = ses->visible_title_bar;
		set_screen_dirty(ses->tab->term->screen, 0, ses->tab->term->height);
	}

	if (prev_status_bar != ses->visible_status_bar) {
		prev_status_bar = ses->visible_status_bar;
		set_screen_dirty(ses->tab->term->screen, 0, ses->tab->term->height);
	}

	if (prev_tabs_bar != ses->visible_tabs_bar) {
		prev_tabs_bar = ses->visible_tabs_bar;
		set_screen_dirty(ses->tab->term->screen, 0, ses->tab->term->height);
	}

	if (doo) {
		doo->x = 0;
		doo->y = 0;
		if (ses->visible_title_bar) doo->y = 1;
		doo->width = ses->tab->term->width;
		doo->height = ses->tab->term->height;
		if (ses->visible_title_bar) doo->height--;
		if (ses->visible_status_bar) doo->height--;
		if (ses->visible_tabs_bar) doo->height--;
	}
}

static struct download *
get_current_download(struct session *ses)
{
	struct download *stat = NULL;

	if (!ses) return NULL;

	if (ses->task)
		stat = &ses->loading;
	else if (have_location(ses))
		stat = &cur_loc(ses)->download;

	if (stat && stat->state == S_OK) {
		struct file_to_load *ftl;

		foreach (ftl, ses->more_files)
			if (ftl->req_sent && ftl->stat.state >= 0)
				return &ftl->stat;
	}

	/* Note that @stat isn't necessarily NULL here,
	 * if @ses->more_files is empty. -- Miciah */
	return stat;
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
		struct download *stat = get_current_download(ses);
		struct color_pair *text_color = NULL;

		if (stat) {
			/* Show S_INTERRUPTED message *once* but then show links
			 * again as usual. */
			if (current_frame(ses)) {
				int ncl = current_frame(ses)->vs->current_link;

				if (stat->state == S_INTERRUPTED
				    && ncl != last_current_link)
					stat->state = S_OK;
				last_current_link = ncl;

				if (stat->state == S_OK)
					msg = print_current_link(ses);
			}

			if (!msg)
				msg = get_stat_msg(stat, term);
		}

		draw_area(term, 0, term->height - 1, term->width, 1, ' ', 0,
			  get_bfu_color(term, "status.status-bar"));

		if (!ses->visible_tabs_bar && tabs_count > 1) {
			unsigned char tab_info[8];

			tab_info[tab_info_len++] = '[';
			ulongcat(tab_info, &tab_info_len, term->current_tab + 1, 4, 0);
			tab_info[tab_info_len++] = ']';
			tab_info[tab_info_len++] = ' ';
			tab_info[tab_info_len] = '\0';

			text_color = get_bfu_color(term, "status.status-text");
			draw_text(term, 0, term->height - 1, tab_info, tab_info_len,
				  0, text_color);
		}

		if (msg) {
			if (!text_color)
				text_color = get_bfu_color(term, "status.status-text");

			draw_text(term, 0 + tab_info_len, term->height - 1,
				  msg, strlen(msg), 0, text_color);
			mem_free(msg);
		}
	}

	if (ses->visible_tabs_bar) {
		struct color_pair *normal_color = get_bfu_color(term, "tabs.normal");
		struct color_pair *selected_color = get_bfu_color(term, "tabs.selected");
		struct color_pair *loading_color = get_bfu_color(term, "tabs.loading");
		struct color_pair *tabsep_color = get_bfu_color(term, "tabs.separator");
		int tab_width = int_max(1, term->width / tabs_count);
		int tab_total_width = tab_width * tabs_count;
		int tab_remain_width = int_max(0, term->width - tab_total_width);
		int tab_num;
		int ypos = term->height - (ses->visible_status_bar ? 2 : 1);
		int xpos = 0;

		for (tab_num = 0; tab_num < tabs_count; tab_num++) {
			struct color_pair *color;
			struct window *tab = get_tab_by_number(term, tab_num);
			struct document_view *doc_view;
			int actual_tab_width = tab_width;
			int msglen;

			/* Adjust tab size to use full term width. */
			if (tab_remain_width) {
				actual_tab_width++;
				tab_remain_width--;
				if (tab_num == tabs_count - 1) {
					actual_tab_width += tab_remain_width;
				}
			}

			doc_view = tab->data ? current_frame(tab->data) : NULL;

			if (doc_view) {
				if (doc_view->document->title
				    && *(doc_view->document->title))
					msg = doc_view->document->title;
				else
					msg = _("Untitled", term);
			} else {
				msg = _("No document", term);
			}

			if (tab_num) {
				draw_text(term, xpos, ypos, "|", 1, 0, tabsep_color);
				xpos += 1;
			}

			/* TODO: fresh_color, for tabs that have not been
			 * selected since they completed loading. -- Miciah */
			if (tab_num == term->current_tab) {
				color = selected_color;
			} else {
				struct download *stat;

				stat = get_current_download(tab->data);

				if (stat && stat->state != S_OK)
					color = loading_color;
				else
					color = normal_color;
			}

			draw_area(term, xpos, ypos, actual_tab_width, 1, ' ', 0, color);

			msglen = strlen(msg);
			if (msglen >= actual_tab_width)
				msglen = actual_tab_width - 1;

			draw_text(term, xpos, ypos, msg, msglen, 0, color);
			tab->xpos = xpos;
			tab->width = actual_tab_width;
			xpos += actual_tab_width;
		}
	}

	if (ses_tab_is_current && ses->visible_title_bar) {
		draw_area(term, 0, 0, term->width, 1, ' ', 0,
			  get_bfu_color(term, "title.title-bar"));

		if (current_frame(ses)) {
			msg = print_current_title(ses);
			if (msg) {
				int msglen = strlen(msg);
				int pos = term->width - 1 - msglen;

				if (pos < 0) pos = 0;
				draw_text(term, pos, 0, msg, msglen, 0,
					  get_bfu_color(term, "title.title-text"));
				mem_free(msg);
			}
		}
	}

	if (!ses_tab_is_current) goto title_set;
	msg = stracpy("ELinks");
	if (msg) {
		int msglen;
		static void *last_ses = NULL;

		if (ses->doc_view && ses->doc_view->document
		    && ses->doc_view->document->title
		    && ses->doc_view->document->title[0]) {
			add_to_strn(&msg, " - ");
			add_to_strn(&msg, ses->doc_view->document->title);
		}

		msglen = strlen(msg);
		if (last_ses != ses
		    || !ses->last_title
		    || strlen(ses->last_title) != msglen
		    || memcmp(ses->last_title, msg, msglen)) {
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
print_error_dialog(struct session *ses, struct download *stat)
{
	unsigned char *t = get_err_msg(stat->state, ses->tab->term);

	if (!t) return;
	msg_box(ses->tab->term, NULL, MSGBOX_NO_INTL,
		_("Error", ses->tab->term), AL_CENTER,
		t,
		ses, 1,
		_("OK", ses->tab->term), NULL, B_ENTER | B_ESC /*,
		N_("Retry", ses->tab->term), NULL, 0 */ /* !!! TODO: retry */);
}

static void
free_task(struct session *ses)
{
	assertm(ses->task, "Session has no task");
	if_assert_failed return;

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
		if (ftl->ce) cache_entry_unlock(ftl->ce);
		if (ftl->url) mem_free(ftl->url);
	}
	free_list(ses->more_files);
}


void
ses_forward(struct session *ses)
{
	struct location *loc;
	int len;

	free_files(ses);

	if (ses->search_word) {
		mem_free(ses->search_word);
		ses->search_word = NULL;
	}

x:
	len = strlen(ses->loading_url);
	if (have_location(ses))
		int_lower_bound(&len, strlen(cur_loc(ses)->vs.url));

	/* struct view_state reserves one byte, so len is sufficient. */
	loc = mem_alloc(sizeof(struct location) + len);
	if (!loc) return;
	memset(loc, 0, sizeof(struct location));
	memcpy(&loc->download, &ses->loading, sizeof(struct download));

	if (ses->task_target_frame && *ses->task_target_frame) {
		struct frame *frame;

		assertm(have_location(ses), "no location yet");
		if_assert_failed return;

		copy_location(loc, cur_loc(ses));
		add_to_history(&ses->history, loc);
		frame = ses_change_frame_url(ses, ses->task_target_frame,
					     ses->loading_url);

		if (!frame) {
			del_from_history(&ses->history, loc);
			destroy_location(loc);
			ses->task_target_frame = NULL;
			goto x;
		}

		destroy_vs(&frame->vs);
		init_vs(&frame->vs, ses->loading_url);

		if (ses->goto_position) {
			if (frame->vs.goto_position)
				mem_free(frame->vs.goto_position);
			frame->vs.goto_position = ses->goto_position;
			ses->goto_position = NULL;
		}
#if 0
		request_additional_loading_file(ses, ses->loading_url,
						&ses->loading, PRI_FRAME);
#endif
	} else {
		init_list(loc->frames);
		init_vs(&loc->vs, ses->loading_url);
		add_to_history(&ses->history, loc);

		if (ses->goto_position) {
			loc->vs.goto_position = ses->goto_position;
			ses->goto_position = NULL;
		}
	}

	/* This is another "branch" in the browsing, so throw away the current
	 * unhistory, we are venturing in another direction! */
	if (ses->task == TASK_FORWARD)
		clean_unhistory(&ses->history);
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


void file_end_load(struct download *, struct file_to_load *);
void abort_preloading(struct session *, int);

struct task {
	struct session *ses;
	unsigned char *url;
	int pri;
	enum cache_mode cache_mode;
	enum task_type type;
	unsigned char *target_frame;
	struct location *target_location;
	unsigned char *pos;
	void (*fn)(struct download *, struct session *);
};


static void
post_yes(struct task *task)
{
	struct session *ses = task->ses;

	abort_preloading(task->ses, 0);
	if (task->ses->goto_position) mem_free(task->ses->goto_position);

	ses->goto_position = task->pos ? stracpy(task->pos) : NULL;
	ses->loading.end = (void (*)(struct download *, void *))task->fn;
	ses->loading.data = task->ses;
	ses->loading_url = stracpy(task->url);
	ses->task = task->type;
	ses->task_target_frame = task->target_frame;
	ses->task_target_location = task->target_location;

	load_url(ses->loading_url, ses->ref_url,
		 &ses->loading, task->pri, task->cache_mode, -1);
}

static void
post_no(struct task *task)
{
	reload(task->ses, CACHE_MODE_NORMAL);
}

void
ses_goto(struct session *ses, unsigned char *url, unsigned char *target_frame,
	 struct location *target_location,
	 int pri, enum cache_mode cache_mode, enum task_type task_type,
	 unsigned char *pos,
	 void (*fn)(struct download *, struct session *),
	 int redir)
{
	struct task *task = mem_alloc(sizeof(struct task));
	unsigned char *m1, *m2;
	struct cache_entry *e;
	unsigned char *post_char_pos = strchr(url, POST_CHAR);

	if (ses->doc_view
	    && ses->doc_view->document
	    && ses->doc_view->document->refresh) {
		kill_document_refresh(ses->doc_view->document->refresh);
	}

	if (!task
	    || !get_opt_int("document.browse.forms.confirm_submit")
	    || !post_char_pos
	    || (cache_mode == CACHE_MODE_ALWAYS && find_in_cache(url, &e)
		&& !e->incomplete)) {

		if (task) mem_free(task);

		if (ses->goto_position) mem_free(ses->goto_position);
		ses->goto_position = pos;

		ses->loading.end = (void (*)(struct download *, void *)) fn;
		ses->loading.data = ses;
		ses->loading_url = url;
		ses->task = task_type;
		ses->task_target_frame = target_frame;
		ses->task_target_location = target_location;

		load_url(url, ses->ref_url, &ses->loading, pri, cache_mode, -1);

		return;
	}

	task->ses = ses;
	task->url = url;
	task->pri = pri;
	task->cache_mode = cache_mode;
	task->type = task_type;
	task->target_frame = target_frame;
	task->target_location = target_location;
	task->pos = pos;
	task->fn = fn;

	if (redir) {
		m1 = N_("Do you want to follow redirect and post form data "
			"to URL %s?");
	} else if (task_type == TASK_FORWARD) {
		m1 = N_("Do you want to post form data to URL %s?");
	} else {
		m1 = N_("Do you want to repost form data to URL %s?");
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
do_move(struct session *ses, struct download **stat)
{
	struct cache_entry *ce;
	enum protocol protocol;

	assert(stat && *stat);
	assertm(ses->loading_url, "no ses->loading_url");
	if_assert_failed return 0;

	protocol = known_protocol(ses->loading_url, NULL);
	if (protocol == PROTOCOL_UNKNOWN) return 0;

	if (ses->task == TASK_IMGMAP && (*stat)->state >= 0)
		return 0;

	ce = (*stat)->ce;
	if (!ce) return 0;

	if (ce->redirect && ses->redirect_cnt++ < MAX_REDIRECTS) {
		unsigned char *u;
		enum task_type task = ses->task;

		if (task == TASK_HISTORY && !have_location(ses))
			goto b;

		u = join_urls(ses->loading_url, ce->redirect);
		if (!u) goto b;

		if (!ce->redirect_get &&
		    !get_opt_int("protocol.http.bugs.broken_302_redirect")) {
			unsigned char *p = strchr(ses->loading_url, POST_CHAR);

			if (p) add_to_strn(&u, p);
		}
		/* ^^^^ According to RFC2068 POST must not be redirected to GET, but
			some BUGGY message boards rely on it :-( */

		abort_loading(ses, 0);
		if (have_location(ses))
			*stat = &cur_loc(ses)->download;
		else
			*stat = NULL;

		set_referrer(ses, ce->url);

		switch (task) {
		case TASK_NONE:
			break;
		case TASK_FORWARD:
		{
			protocol_external_handler *fn;

			fn = get_protocol_external_handler(protocol);
			if (fn) {
				fn(ses, u);
				mem_free(u);
				*stat = NULL;
				return 0;
			}
		}
			/* Fall through. */
		case TASK_IMGMAP:
			{
			unsigned char *gp = ses->goto_position
					    ? stracpy(ses->goto_position)
					    : NULL;

			ses_goto(ses, u, ses->task_target_frame, NULL,
				 PRI_MAIN, CACHE_MODE_NORMAL, task,
				 gp, end_load, 1);
			if (gp) mem_free(gp);
			return 2;
			}
		case TASK_HISTORY:
			ses_goto(ses, u, NULL, ses->task_target_location,
				 PRI_MAIN, CACHE_MODE_NORMAL, TASK_RELOAD,
				 NULL, end_load, 1);
			return 2;
		case TASK_RELOAD:
			ses_goto(ses, u, NULL, NULL,
				 PRI_MAIN, ses->reloadlevel, TASK_RELOAD,
				 NULL, end_load, 1);
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

	switch (ses->task) {
		case TASK_NONE:
			break;
		case TASK_FORWARD:
			if (ses_chktype(ses, stat, ce)) {
				free_task(ses);
				reload(ses, CACHE_MODE_NORMAL);
				return 2;
			}
			break;
		case TASK_IMGMAP:
			ses_imgmap(ses);
			break;
		case TASK_HISTORY:
			ses_history_move(ses);
			break;
		case TASK_RELOAD:
			ses->task_target_location = cur_loc(ses)->prev;
			ses_history_move(ses);
			ses_forward(ses);
			break;
	}

	if ((*stat)->state >= 0) {
		*stat = &cur_loc(ses)->download;
		change_connection(&ses->loading, *stat, PRI_MAIN, 0);
	} else {
		cur_loc(ses)->download.state = ses->loading.state;
	}

	free_task(ses);
	return 1;
}


void request_frameset(struct session *, struct frameset_desc *);

static void
request_frame(struct session *ses, unsigned char *name, unsigned char *uurl)
{
	struct location *loc = cur_loc(ses);
	struct frame *frame;
	unsigned char *url, *pos;

	assertm(have_location(ses), "request_frame: no location");
	if_assert_failed return;

	foreach (frame, loc->frames) {
		struct document_view *doc_view;

		if (strcasecmp(frame->name, name))
			continue;

		foreach(doc_view, ses->scrn_frames) {
			if (doc_view->vs == &frame->vs && doc_view->document->frame_desc) {
				request_frameset(ses, doc_view->document->frame_desc);
				return;
			}
		}

		url = memacpy(frame->vs.url, frame->vs.url_len);
		if (!url) return;
#if 0
		/* This seems not to be needed anymore, it looks like this
		 * condition should never happen. It's apparently what Mikulas
		 * thought, at least. I'll review this more carefully when I
		 * will understand this stuff better ;-). --pasky */
		if (frame->vs.view && frame->vs.view->document
		    && frame->vs.view->document->frame_desc)) {
			request_frameset(ses, frame->vs.view->document->frame_desc);
			if (url) mem_free(url);
			return;
		}
#endif
		goto found;
	}

	url = stracpy(uurl);
	if (!url) return;
	pos = extract_position(url);

	/* strlen(url) without + 1 since vs have already reserved one byte. */
	frame = mem_alloc(sizeof(struct frame) + strlen(url));
	if (!frame) {
		mem_free(url);
		if (pos) mem_free(pos);
		return;
	}
	memset(frame, 0, sizeof(struct frame));

	frame->name = stracpy(name);
	if (!frame->name) {
		mem_free(frame);
		mem_free(url);
		if (pos) mem_free(pos);
		return;
	}

	init_vs(&frame->vs, url);
	if (pos) frame->vs.goto_position = pos;

	add_to_list(loc->frames, frame);

found:
	if (*url)
		request_additional_file(ses, url, PRI_FRAME);
	mem_free(url);
}

void
request_frameset(struct session *ses, struct frameset_desc *frameset_desc)
{
	static int depth = 0; /* Inheritation counter (recursion brake ;) */

	if (++depth <= HTML_MAX_FRAME_DEPTH) {
		int i = 0;

		for (; i < frameset_desc->n; i++) {
			struct frame_desc *frame_desc = &frameset_desc->frame_desc[i];

			if (frame_desc->subframe) {
				request_frameset(ses, frame_desc->subframe);
			} else if (frame_desc->name) {
				request_frame(ses, frame_desc->name,
					      frame_desc->url);
			}
		}
	}

	depth--;
}

inline void
load_frames(struct session *ses, struct document_view *doc_view)
{
	struct document *document = doc_view->document;

	if (!document || !document->frame_desc) return;
	request_frameset(ses, document->frame_desc);
}

void
display_timer(struct session *ses)
{
	ttime t = get_time();
	draw_formatted(ses);

	t = (get_time() - t) * DISPLAY_TIME;
	if (t < DISPLAY_TIME_MIN) t = DISPLAY_TIME_MIN;

	ses->display_timer = install_timer(t, (void (*)(void *))display_timer,
					   ses);
	load_frames(ses, ses->doc_view);
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
end_load(struct download *stat, struct session *ses)
{
	int d;

	assertm(ses->task, "end_load: no ses->task");
	if_assert_failed return;

	d = do_move(ses, &stat);
	if (!stat) return;
	if (d == 2) goto end;

	if (d == 1) {
		stat->end = (void (*)(struct download *, void *))doc_end_load;
		display_timer(ses);
	}

	if (stat->state < 0) {
		if (ses->task) free_task(ses);
		if (d == 1) doc_end_load(stat, ses);
	}

	if (stat->state < 0 && stat->state != S_OK) {
		print_error_dialog(ses, stat);
		if (d == 0) reload(ses, CACHE_MODE_NORMAL);
	}

end:
	check_questions_queue(ses);
	print_screen_status(ses);
}

#ifdef HAVE_SCRIPTING
static void
maybe_pre_format_html(struct cache_entry *ce, struct session *ses)
{
	struct fragment *fr;
	unsigned char *src;
	int len;
	static int pre_format_html_event = EVENT_NONE;

	if (!ce || ce->done_pre_format_html_hook || list_empty(ce->frag))
		return;

	defrag_entry(ce);
	fr = ce->frag.next;
	src = fr->data;
	len = fr->length;

	set_event_id(pre_format_html_event, "pre-format-html");
	trigger_event(pre_format_html_event, &src, &len, ses, ce->url);

	if (src && src != fr->data) {
		add_fragment(ce, 0, src, len);
		truncate_entry(ce, len, 1);
		ce->incomplete = 0; /* XXX */
		mem_free(src);
	}

	ce->done_pre_format_html_hook = 1;
}
#endif

void
doc_end_load(struct download *stat, struct session *ses)
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

		draw_formatted(ses);

		if (get_opt_bool_tree(cmdline_options, "auto-submit")) {
			if (!list_empty(ses->doc_view->document->forms)) {
				get_opt_bool_tree(cmdline_options,
						  "auto-submit") = 0;
				submit = 1;
			}
		}

		load_frames(ses, ses->doc_view);
		process_file_requests(ses);

		if (ses->doc_view->document->refresh
		    && get_opt_bool("document.browse.refresh")) {
			start_document_refresh(ses->doc_view->document->refresh,
					       ses);
		}

		if (stat->state != S_OK) {
			print_error_dialog(ses, stat);
		}

	} else if (ses->display_timer == -1) {
		display_timer(ses);
	}

	check_questions_queue(ses);
	print_screen_status(ses);

#ifdef GLOBHIST
	if (stat->conn)
		add_global_history_item(struri(stat->conn->uri),
					ses->doc_view->document->title,
					time(NULL));
#endif

	if (submit) {
		goto_link(get_form_url(ses, ses->doc_view, fc), fc->target, ses,
			  1);
	}
}

void
file_end_load(struct download *stat, struct file_to_load *ftl)
{
	if (ftl->stat.ce) {
		if (ftl->ce) cache_entry_unlock(ftl->ce);
		ftl->ce = ftl->stat.ce;
		cache_entry_lock(ftl->ce);
	}

	/* FIXME: We need to do content-type check here! However, we won't
	 * handle properly the "Choose action" dialog now :(. */
#if 0
	if (ses_chktype(ftl->ses, stat, ftl->ce)) {
#if 0
		free_wtd(ftl->ses);
		reload(ses, CACHE_MODE_NORMAL);
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

	ftl = mem_calloc(1, sizeof(struct file_to_load));
	if (!ftl) return NULL;

	ftl->url = stracpy(url);
	if (!ftl->url) {
		mem_free(ftl);
		return NULL;
	}

	ftl->stat.end = (void (*)(struct download *, void *)) file_end_load;
	ftl->stat.data = ftl;
	ftl->pri = pri;
	ftl->ses = ses;

	add_to_list(ses->more_files, ftl);

	return ftl;
}

struct file_to_load *
request_additional_loading_file(struct session *ses, unsigned char *url,
				struct download *stat, int pri)
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
	struct document_view *doc_view = current_frame(ses);
	int more = 1;

	if (stop_recursion) return;
	stop_recursion = 1;

	while (more) {
		more = 0;
		foreach (ftl, ses->more_files) {
			unsigned char *referer = NULL;

			if (ftl->req_sent)
				continue;

			ftl->req_sent = 1;
			if (doc_view && doc_view->document)
				referer = doc_view->document->url;

			load_url(ftl->url, referer,
				 &ftl->stat, ftl->pri, CACHE_MODE_NORMAL, -1);
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

	create_history(&ses->history);
	init_list(ses->scrn_frames);
	init_list(ses->more_files);
	ses->tab = tab;
	ses->id = session_id++;
	ses->task = TASK_NONE;
	ses->display_timer = -1;

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
		get_opt_rec(config_options, "config.saving_style_w")->flags |= OPT_TOUCHED;
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
	if (!have_location(old)) return;

	goto_url(new, cur_loc(old)->vs.url);
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
	if (l) memcpy(i + 2, url, l);

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
	if (url_len && len >= 2 * sizeof(int) + url_len) {
		unsigned char *url = fmem_alloc(url_len + 1);

		if (!url) return info;

		memcpy(url, data, url_len);
		url[url_len] = '\0';

		info->url = decode_shell_safe_url(url);

		fmem_free(url);
	}

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
	dialog_goto_url((struct session *) data, NULL);
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
	if (!ses->task) return;

	change_connection(&ses->loading, NULL, PRI_CANCEL, interrupt);
	free_task(ses);
}

void
abort_loading(struct session *ses, int interrupt)
{
	if (have_location(ses)) {
		struct location *l = cur_loc(ses);

		if (l->download.state >= 0)
			change_connection(&l->download, NULL, PRI_CANCEL, interrupt);
		abort_files_load(ses, interrupt);
	}
	abort_preloading(ses, interrupt);
}

static void
destroy_session(struct session *ses)
{
	struct document_view *doc_view;

	assert(ses);
	if_assert_failed return;

	destroy_downloads(ses);
	abort_loading(ses, 0);
	free_files(ses);
	if (ses->doc_view) {
		detach_formatted(ses->doc_view);
		mem_free(ses->doc_view);
	}

	foreach (doc_view, ses->scrn_frames)
		detach_formatted(doc_view);

	free_list(ses->scrn_frames);

	destroy_history(&ses->history);
	set_referrer(ses, NULL);

	if (ses->loading_url) mem_free(ses->loading_url);
	if (ses->display_timer != -1) kill_timer(ses->display_timer);
	if (ses->goto_position) mem_free(ses->goto_position);
	if (ses->imgmap_href_base) mem_free(ses->imgmap_href_base);
	if (ses->imgmap_target_base) mem_free(ses->imgmap_target_base);
	if (ses->tq_ce) cache_entry_unlock(ses->tq_ce);
	if (ses->tq_url) {
		change_connection(&ses->tq, NULL, PRI_CANCEL, 0);
		mem_free(ses->tq_url);
	}
	if (ses->tq_goto_position) mem_free(ses->tq_goto_position);
	if (ses->tq_prog) mem_free(ses->tq_prog);
	if (ses->dn_url) mem_free(ses->dn_url);
	if (ses->search_word) mem_free(ses->search_word);
	if (ses->last_search_word) mem_free(ses->last_search_word);
	if (ses->last_title) mem_free(ses->last_title);
	del_from_list(ses);
}

void
reload(struct session *ses, enum cache_mode cache_mode)
{
	abort_loading(ses, 0);

	if (cache_mode == CACHE_MODE_INCREMENT) {
		cache_mode = CACHE_MODE_NEVER;
		if (ses->reloadlevel < CACHE_MODE_NEVER)
			cache_mode = ++ses->reloadlevel;
	} else {
		ses->reloadlevel = cache_mode;
	}

	if (have_location(ses)) {
		struct location *l = cur_loc(ses);
		struct file_to_load *ftl;
		struct document_view *doc_view = current_frame(ses);

		l->download.data = ses;
		l->download.end = (void *)doc_end_load;
		load_url(l->vs.url, ses->ref_url, &l->download, PRI_MAIN, cache_mode, -1);
		foreach (ftl, ses->more_files) {
			unsigned char *referer = NULL;

			if (ftl->req_sent && ftl->stat.state >= 0) continue;
			ftl->stat.data = ftl;
			ftl->stat.end = (void *)file_end_load;

			if (doc_view && doc_view->document)
				referer = doc_view->document->url;

			load_url(ftl->url, referer,
				 &ftl->stat, PRI_FRAME, cache_mode, -1);
		}
	}
}

#if 0
void
ses_load_notify(struct download *stat, struct session *ses)
{
	if (stat->state == S_TRANS || stat->state == S_OK) {
		stat->end = (void (*)(struct download *, void *))end_load;
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
print_unknown_protocol_dialog(struct session *ses)
{
	msg_box(ses->tab->term, NULL, 0,
		N_("Error", ses->tab->term), AL_CENTER,
		N_("This URL contains a protocol not yet known by ELinks.\n"
		   "You can configure an external handler for it through options system."),
		ses, 1,
		N_("OK", ses->tab->term), NULL, B_ENTER | B_ESC);
}

static void
do_follow_url(struct session *ses, unsigned char *url, unsigned char *target,
	      enum task_type task, enum cache_mode cache_mode, int do_referrer)
{
	unsigned char *u, *referrer = NULL;
	unsigned char *pos;
	protocol_external_handler *fn;
	enum protocol protocol = known_protocol(url, NULL);

	if (protocol == PROTOCOL_UNKNOWN) {
		print_unknown_protocol_dialog(ses);
		return;
	}

	if (protocol != PROTOCOL_INVALID) {
		fn = get_protocol_external_handler(protocol);
		if (fn) {
			fn(ses, url);
			return;
		}
	}

	ses->reloadlevel = cache_mode;

	u = translate_url(url, ses->tab->term->cwd);
	if (!u) {
		struct download stat = { NULL_LIST_HEAD, NULL, NULL,
					 NULL, NULL, NULL,
					 S_BAD_URL, PRI_CANCEL, 0 };

		print_error_dialog(ses, &stat);
		return;
	}

	pos = extract_position(u);

	if (ses->task == task) {
		if (!strcmp(ses->loading_url, u)) {
			/* We're already loading the URL. */
			mem_free(u);

			if (ses->goto_position)
				mem_free(ses->goto_position);
			ses->goto_position = pos;

			return;
		}
	}

	abort_loading(ses, 0);

	if (do_referrer) {
		struct document_view *doc_view = current_frame(ses);

		if (doc_view && doc_view->document)
			referrer = doc_view->document->url;
	}

	set_referrer(ses, referrer);

	ses_goto(ses, u, target, NULL,
		 PRI_MAIN, cache_mode, task,
		 pos, end_load, 0);

	/* abort_loading(ses); */
}

static void
follow_url(struct session *ses, unsigned char *url, unsigned char *target,
	   enum task_type task, enum cache_mode cache_mode, int referrer)
{
	unsigned char *new_url = url;
#ifdef HAVE_SCRIPTING
	static int follow_url_event_id = EVENT_NONE;

	set_event_id(follow_url_event_id, "follow-url");
	trigger_event(follow_url_event_id, &new_url, ses);
	if (!new_url) return;
#endif

	if (*new_url)
		do_follow_url(ses, new_url, target, task, cache_mode, referrer);

#ifdef HAVE_SCRIPTING
	if (new_url != url) mem_free(new_url);
#endif
}

void
goto_url_frame_reload(struct session *ses, unsigned char *url,
		      unsigned char *target)
{
	follow_url(ses, url, target, TASK_FORWARD, CACHE_MODE_FORCE_RELOAD, 1);
}

void
goto_url_frame(struct session *ses, unsigned char *url,
	       unsigned char *target)
{
	follow_url(ses, url, target, TASK_FORWARD, CACHE_MODE_NORMAL, 1);
}

void
goto_url(struct session *ses, unsigned char *url)
{
	follow_url(ses, url, NULL, TASK_FORWARD, CACHE_MODE_NORMAL, 0);
}

void
goto_url_with_hook(struct session *ses, unsigned char *url)
{
	unsigned char *new_url = url;
#ifdef HAVE_SCRIPTING
	static int goto_url_event_id = EVENT_NONE;

	set_event_id(goto_url_event_id, "goto-url");
	trigger_event(goto_url_event_id, &new_url, ses);
	if (!new_url) return;
#endif

	if (*new_url) goto_url(ses, new_url);

#ifdef HAVE_SCRIPTING
	if (new_url != url) mem_free(new_url);
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
	follow_url(ses, url, target, TASK_IMGMAP, CACHE_MODE_NORMAL, 1);
}

struct frame *
ses_find_frame(struct session *ses, unsigned char *name)
{
	struct location *loc = cur_loc(ses);
	struct frame *frame;

	assertm(have_location(ses), "ses_request_frame: no location yet");
	if_assert_failed return NULL;

	foreachback (frame, loc->frames)
		if (!strcasecmp(frame->name, name))
			return frame;

	return NULL;
}

struct frame *
ses_change_frame_url(struct session *ses, unsigned char *name,
		     unsigned char *url)
{
	struct location *loc = cur_loc(ses);
	struct frame *frame;
	size_t url_len = strlen(url);

	assertm(have_location(ses), "ses_change_frame_url: no location yet");
	if_assert_failed { return NULL; }

	foreachback (frame, loc->frames) {
		if (strcasecmp(frame->name, name)) continue;

		if (url_len > frame->vs.url_len) {
			struct document_view *doc_view;
			struct frame *new_frame = frame;

			/* struct view_state reserves 1 byte for url, so
			 * url_len is sufficient. */
			new_frame = mem_realloc(frame, sizeof(struct frame) + url_len);
			if (!new_frame) return NULL;

			new_frame->prev->next = new_frame->next->prev = new_frame;

			foreach (doc_view, ses->scrn_frames)
				if (doc_view->vs == &frame->vs)
					doc_view->vs = &new_frame->vs;

			frame = new_frame;
		}
		memcpy(frame->vs.url, url, url_len + 1);
		frame->vs.url_len = url_len;

		return frame;
	}

	return NULL;

}

void
tabwin_func(struct window *tab, struct term_event *ev, int fw)
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
			draw_formatted(ses);
			load_frames(ses, ses->doc_view);
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
	int_upper_bound(&url_len, str_size - 1);

	return safe_strncpy(str, here, url_len + 1);
}


/*
 * Gets the title of the page being viewed by this session. Writes it into str.
 * A maximum of str_size bytes (including null) will be written.
 */
unsigned char *
get_current_title(struct session *ses, unsigned char *str, size_t str_size)
{
	struct document_view *doc_view = current_frame(ses);

	/* Ensure that the title is defined */
	if (doc_view && doc_view->document->title)
		return safe_strncpy(str, doc_view->document->title, str_size);

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
	struct document_view *doc_view = current_frame(ses);

	if (doc_view && doc_view->vs->current_link != -1) {
		struct link *link;

		link = &doc_view->document->links[doc_view->vs->current_link];

		/* Only return a hyper text link */
		if (link->type == LINK_HYPERTEXT) return link;
	}

	return NULL;
}
