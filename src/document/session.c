/* Sessions managment - you'll find things here which you wouldn't expect */
/* $Id: session.c,v 1.32 2002/05/08 13:39:00 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <links.h>

#include <bfu/bfu.h>
#include <bfu/menu.h>
#include <config/options.h>
#include <document/cache.h>
#include <document/download.h>
#include <document/history.h>
#include <document/location.h>
#include <document/options.h>
#include <document/session.h>
#include <document/globhist.h>
#include <document/view.h>
#include <document/html/parser.h>
#include <document/html/renderer.h>
#include <intl/language.h>
#include <lowlevel/home.h>
#include <lowlevel/sched.h>
#include <lowlevel/select.h>
#include <lowlevel/terminal.h>
#include <lowlevel/ttime.h>
#include <lua/hooks.h>
#include <protocol/types.h>
#include <protocol/url.h>
#include <util/conv.h>
#include <util/error.h>
#include <util/memlist.h>


void check_questions_queue(struct session *ses);


struct file_to_load {
	struct file_to_load *next;
	struct file_to_load *prev;
	struct session *ses;
	int req_sent;
	int pri;
	struct cache_entry *ce;
	unsigned char *url;
	struct status stat;
};

struct file_to_load *request_additional_file(struct session *, unsigned char *, int);
struct file_to_load *request_additional_loading_file(struct session *, unsigned char *, struct status *, int);


struct list_head sessions = {&sessions, &sessions};

int session_id = 1;


struct strerror_val {
	struct strerror_val *next;
	struct strerror_val *prev;
	unsigned char msg[1];
};

struct list_head strerror_buf = { &strerror_buf, &strerror_buf };

void free_strerror_buf()
{
	free_list(strerror_buf);
}

unsigned char *get_err_msg(int state)
{
	unsigned char *e;
	struct strerror_val *s;
	if (state <= S_OK || state >= S_WAIT) {
		int i;
		for (i = 0; msg_dsc[i].msg; i++)
			if (msg_dsc[i].n == state) return msg_dsc[i].msg;
		unk:
		return TEXT(T_UNKNOWN_ERROR);
	}
	if (!(e = strerror(-state)) || !*e) goto unk;
	foreach(s, strerror_buf) if (!strcmp(s->msg, e)) return s->msg;
	if (!(s = mem_alloc(sizeof(struct strerror_val) + strlen(e) + 1))) goto unk;
	strcpy(s->msg, e);
	add_to_list(strerror_buf, s);
	return s->msg;
}


void add_xnum_to_str(unsigned char **s, int *l, int n)
{
	unsigned char suff = 0;
	int d = -1;
	if (n >= 1000000) suff = 'M', d = (n / 100000) % 10, n /= 1000000;
	else if (n >= 1000) suff = 'k', d = (n / 100) % 10, n /= 1000;
	add_num_to_str(s, l, n);
	if (n < 10 && d != -1) add_chr_to_str(s, l, '.'), add_num_to_str(s, l, d);
	add_chr_to_str(s, l, ' ');
	if (suff) add_chr_to_str(s, l, suff);
	add_chr_to_str(s, l, 'B');
}

void add_time_to_str(unsigned char **s, int *l, ttime t)
{
	unsigned char q[64];
	t /= 1000;
	t &= 0xffffffff;
	if (t < 0) t = 0;
	if (t >= 86400) sprintf(q, "%dd ", (int)(t / 86400)), add_to_str(s, l, q);
	if (t >= 3600) t %= 86400, sprintf(q, "%d:%02d", (int)(t / 3600), (int)(t / 60 % 60)), add_to_str(s, l, q);
	else sprintf(q, "%d", (int)(t / 60)), add_to_str(s, l, q);
	sprintf(q, ":%02d", (int)(t % 60)), add_to_str(s, l, q);
}

unsigned char *get_stat_msg(struct status *stat, struct terminal *term)
{
	if (stat->state == S_TRANS && stat->prg->elapsed / 100) {
		unsigned char *m = init_str();
		int l = 0;
		add_to_str(&m, &l, _(TEXT(T_RECEIVED), term));
		add_to_str(&m, &l, " ");
		add_xnum_to_str(&m, &l, stat->prg->pos);
		if (stat->prg->size >= 0)
			add_to_str(&m, &l, " "), add_to_str(&m, &l, _(TEXT(T_OF), term)), add_to_str(&m, &l, " "), add_xnum_to_str(&m, &l, stat->prg->size);
		add_to_str(&m, &l, ", ");
		if (stat->prg->elapsed >= CURRENT_SPD_AFTER * SPD_DISP_TIME)
			add_to_str(&m, &l, _(TEXT(T_AVG), term)), add_to_str(&m, &l, " ");
		add_xnum_to_str(&m, &l, (longlong)stat->prg->loaded * 10 / (stat->prg->elapsed / 100));
		add_to_str(&m, &l, "/s");
		if (stat->prg->elapsed >= CURRENT_SPD_AFTER * SPD_DISP_TIME)
			add_to_str(&m, &l, ", "), add_to_str(&m, &l, _(TEXT(T_CUR), term)), add_to_str(&m, &l, " "),
			add_xnum_to_str(&m, &l, stat->prg->cur_loaded / (CURRENT_SPD_SEC * SPD_DISP_TIME / 1000)),
			add_to_str(&m, &l, "/s");
		return m;
	}
	return stracpy(_(get_err_msg(stat->state), term));
}

/* Print statusbar and titlebar, set terminal title. */
void print_screen_status(struct session *ses)
{
	struct terminal *term = ses->term;
	struct status *stat = NULL;
	unsigned char *msg = NULL;

	/* TODO: Make this optionally switchable off. */

	if (show_title_bar)
		fill_area(term, 0, 0, term->x, 1, COLOR_TITLE_BG);
	if (show_status_bar)
		fill_area(term, 0, term->y - 1, term->x, 1, COLOR_STATUS_BG);

	if (ses->wtd)
		stat = &ses->loading;
	else if (have_location(ses))
		stat = &cur_loc(ses)->stat;

	if (stat && stat->state == S_OK) {
		struct file_to_load *ftl;

		foreach(ftl, ses->more_files) {
			if (ftl->req_sent && ftl->stat.state >= 0) {
				stat = &ftl->stat;
				break;
			}
		}
	}

	if (stat) {
		if (show_status_bar) {
			if (stat->state == S_OK)
				msg = print_current_link(ses);
			if (!msg)
				msg = get_stat_msg(stat, term);
			if (msg) {
				print_text(term, 0, term->y - 1, strlen(msg),
					   msg, COLOR_STATUS);
				mem_free(msg);
			}
		}

		if (show_title_bar) {
			msg = print_current_title(ses);
			if (msg) {
				int pos = term->x - 1 - strlen(msg);

				if (pos < 0) pos = 0;
				print_text(term, pos, 0, strlen(msg),
					   msg, COLOR_TITLE);
				mem_free(msg);
			}
		}

		msg = stracpy("ELinks");
		if (msg) {
			if (ses->screen && ses->screen->f_data
			    && ses->screen->f_data->title
			    && ses->screen->f_data->title[0]) {
				add_to_strn(&msg, " - ");
				add_to_strn(&msg, ses->screen->f_data->title);
			}
			set_terminal_title(term, msg);
			mem_free(msg);
		}
	}

	redraw_from_window(ses->win);
}

void print_error_dialog(struct session *ses, struct status *stat, unsigned char *title)
{
	unsigned char *t = get_err_msg(stat->state);
	if (!t) return;
	msg_box(ses->term, NULL,
		title, AL_CENTER,
		t,
		ses, 1,
		TEXT(T_CANCEL), NULL, B_ENTER | B_ESC /*,
		_("Retry"), NULL, 0 */ /* !!! FIXME: retry */);
}

void free_wtd(struct session *ses)
{
	if (!ses->wtd) {
		internal("no WTD");
		return;
	}
	if (ses->goto_position) mem_free(ses->goto_position), ses->goto_position = NULL;
	mem_free(ses->loading_url);
	ses->loading_url = NULL;
	ses->wtd = WTD_NO;
}

void abort_files_load(struct session *ses)
{
	struct file_to_load *ftl;
	int q;
	do {
		q = 0;
		foreach(ftl, ses->more_files) {
			if (ftl->stat.state >= 0 && ftl->req_sent) {
				q = 1;
				change_connection(&ftl->stat, NULL, PRI_CANCEL);
			}
		}
	} while (q);
}

void free_files(struct session *ses)
{
	struct file_to_load *ftl;
	abort_files_load(ses);
	foreach(ftl, ses->more_files) {
		if (ftl->ce) ftl->ce->refcount--;
		mem_free(ftl->url);
	}
	free_list(ses->more_files);
}


void ses_forward(struct session *ses)
{
	struct location *l;
	int len;
	free_files(ses);
	if (have_location(ses)) {
		struct frame *frm;
		l = cur_loc(ses);
		foreach(frm, l->frames) frm->vs.f = NULL;
		l->vs.f = NULL;
	}
	if (ses->search_word) mem_free(ses->search_word), ses->search_word = NULL;
	x:
	len = strlen(ses->loading_url);
	if (have_location(ses) && len < strlen(cur_loc(ses)->vs.url))
		len = strlen(cur_loc(ses)->vs.url);
	if (!(l = mem_alloc(sizeof(struct location) + len + 1))) return;
	memset(l, 0, sizeof(struct location));
	memcpy(&l->stat, &ses->loading, sizeof(struct status));
	if (ses->wtd_target && *ses->wtd_target) {
		struct frame *frm;
		if (!have_location(ses)) {
			internal("no location yet");
			return;
		}
		copy_location(l, cur_loc(ses));
		add_to_history(ses, l);
		frm = ses_change_frame_url(ses, ses->wtd_target, ses->loading_url);
		if (!frm) {
			destroy_location(l);
			ses->wtd_target = NULL;
			goto x;
		}
		destroy_vs(&frm->vs);
		init_vs(&frm->vs, ses->loading_url);
		if (ses->goto_position) {
			if (frm->vs.goto_position) mem_free(frm->vs.goto_position);
			frm->vs.goto_position = ses->goto_position;
			ses->goto_position = NULL;
		}
		/* request_additional_loading_file(ses, ses->loading_url, &ses->loading, PRI_FRAME); */
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

void ses_imgmap(struct session *ses)
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
	if (get_image_map(ce->head, fr->data, fr->data + fr->length, ses->goto_position, &menu, &ml, ses->imgmap_href_base, ses->imgmap_target_base, ses->term->spec->charset, ses->ds.assume_cp, ses->ds.hard_assume))
		return;
	add_empty_window(ses->term, (void (*)(void *))freeml, ml);
	do_menu(ses->term, menu, ses);
}

void map_selected(struct terminal *term, struct link_def *ld, struct session *ses)
{
	goto_url_frame(ses, ld->link, ld->target);
}

void file_end_load(struct status *, struct file_to_load *);
void abort_preloading(struct session *);

struct wtd_data {
	struct session *ses;
	unsigned char *url;
	int pri;
	enum cache_mode cache_mode;
	int wtd;
	unsigned char *target;
	unsigned char *pos;
	void (*fn)(struct status *, struct session *);
};

void post_yes(struct wtd_data *w)
{
	abort_preloading(w->ses);
	if (w->ses->goto_position) mem_free(w->ses->goto_position);
	w->ses->goto_position = stracpy(w->pos);
	w->ses->loading.end = (void (*)(struct status *, void *))w->fn;
	w->ses->loading.data = w->ses;
	w->ses->loading_url = stracpy(w->url);
	w->ses->wtd = w->wtd;
	w->ses->wtd_target = w->target;
	load_url(w->ses->loading_url, w->ses->ref_url, &w->ses->loading, w->pri, w->cache_mode);
}

void post_no(struct wtd_data *w)
{
	*strchr(w->url, POST_CHAR) = 0;
	post_yes(w);
}

void post_cancel(struct wtd_data *w)
{
	reload(w->ses, NC_CACHE);
}

void
ses_goto(struct session *ses, unsigned char *url, unsigned char *target,
	 int pri, enum cache_mode cache_mode, enum session_wtd wtd,
	 unsigned char *pos,
	 void (*fn)(struct status *, struct session *),
	 int redir)
{
	struct wtd_data *wtd_data = mem_alloc(sizeof(struct wtd_data));
	unsigned char *m1, *m2;
	struct cache_entry *e;

	if (!wtd_data || !form_submit_confirm || !strchr(url, POST_CHAR)
	    || (cache_mode == NC_ALWAYS_CACHE && find_in_cache(url, &e)
		&& !e->incomplete)) {

		if (wtd_data) mem_free(wtd_data);

		if (ses->goto_position) mem_free(ses->goto_position);
		ses->goto_position = pos;

		ses->loading.end = (void (*)(struct status *, void *))fn;
		ses->loading.data = ses;
		ses->loading_url = url;
		ses->wtd = wtd;
		ses->wtd_target = target;

		load_url(url, ses->ref_url, &ses->loading, pri, cache_mode);

		return;
	}

	wtd_data->ses = ses;
	wtd_data->url = url;
	wtd_data->pri = pri;
	wtd_data->cache_mode = cache_mode;
	wtd_data->wtd = wtd;
	wtd_data->target = target;
	wtd_data->pos = pos;
	wtd_data->fn = fn;

	if (redir) {
		m1 = TEXT(T_DO_YOU_WANT_TO_FOLLOW_REDIRECT_AND_POST_FORM_DATA_TO_URL);
	} else if (wtd == WTD_FORWARD) {
		m1 = TEXT(T_DO_YOU_WANT_TO_POST_FORM_DATA_TO_URL);
	} else {
		m1 = TEXT(T_DO_YOU_WANT_TO_REPOST_FORM_DATA_TO_URL);
	}

	m2 = memacpy(url, (unsigned char *) strchr(url, POST_CHAR) - url);
	msg_box(ses->term, getml(m2, wtd_data, wtd_data->url, wtd_data->pos,
				 NULL),
		TEXT(T_WARNING), AL_CENTER | AL_EXTD_TEXT,
		m1, " ", m2, "?", NULL,
		wtd_data, 3,
		TEXT(T_YES), post_yes, B_ENTER,
		TEXT(T_NO), post_no, 0,
		TEXT(T_CANCEL), post_cancel, B_ESC);
}

int do_move(struct session *ses, struct status **stat)
{
	struct cache_entry *ce = NULL;
	int l = 0;

	if (!ses->loading_url) {
		internal("no ses->loading_url");
		return 0;
	}

	if (!(ce = (*stat)->ce) || (ses->wtd == WTD_IMGMAP && (*stat)->state >= 0)) {
		return 0;
	}

	if (ce->redirect && ses->redirect_cnt++ < MAX_REDIRECTS) {
		unsigned char *u, *p, *gp;
		enum session_wtd w = ses->wtd;
		if (ses->wtd == WTD_BACK && !have_location(ses))
			goto b;
		if (!(u = join_urls(ses->loading_url, ce->redirect))) goto b;
		if (!http_bugs.bug_302_redirect) if (!ce->redirect_get && (p = strchr(ses->loading_url, POST_CHAR))) add_to_strn(&u, p);
		/* ^^^^ According to RFC2068 POST must not be redirected to GET, but
			some BUGGY message boards rely on it :-( */
		gp = stracpy(ses->goto_position);
		abort_loading(ses);
		if (have_location(ses)) *stat = &cur_loc(ses)->stat;
		else *stat = NULL;
		if (ses->ref_url) mem_free(ses->ref_url);
		ses->ref_url = init_str();
		add_to_str(&ses->ref_url, &l, ce->url);
		if (w == WTD_FORWARD || w == WTD_IMGMAP) {
			ses_goto(ses, u, ses->wtd_target, PRI_MAIN, NC_CACHE, w, gp, end_load, 1);
			return 2;
		}
		if (gp) mem_free(gp);
		if (w == WTD_BACK || w == WTD_UNBACK) {
			ses_goto(ses, u, NULL, PRI_MAIN, NC_CACHE, WTD_RELOAD, NULL, end_load, 1);
			return 2;
		}
		if (w == WTD_RELOAD) {
			ses_goto(ses, u, NULL, PRI_MAIN, ses->reloadlevel, WTD_RELOAD, NULL, end_load, 1);
			return 2;
		}
	} else ses->redirect_cnt = 0;
	b:
	if (ses->display_timer != -1) kill_timer(ses->display_timer), ses->display_timer = -1;
	if (ses->wtd == WTD_FORWARD) {
		if (ses_chktype(ses, stat, ce)) {
			free_wtd(ses);
			reload(ses, NC_CACHE);
			return 2;
		}
	}
	if (ses->wtd == WTD_IMGMAP) ses_imgmap(ses);
	if (ses->wtd == WTD_BACK) ses_back(ses);
	if (ses->wtd == WTD_UNBACK) ses_unback(ses);
	if (ses->wtd == WTD_RELOAD) ses_back(ses), ses_forward(ses);
	if ((*stat)->state >= 0) change_connection(&ses->loading, *stat = &cur_loc(ses)->stat, PRI_MAIN);
	else cur_loc(ses)->stat.state = ses->loading.state;
	free_wtd(ses);
	return 1;
}


void request_frameset(struct session *, struct frameset_desc *);

void
request_frame(struct session *ses, unsigned char *name, unsigned char *uurl)
{
	struct location *loc = cur_loc(ses);
	struct frame *frm;
	unsigned char *url, *pos;

	if (!have_location(ses)) {
		internal("request_frame: no location");
		return;
	}

	foreach(frm, loc->frames) {
		if (strcasecmp(frm->name, name))
			continue;

		url = stracpy(frm->vs.url);
		if (frm->vs.f && frm->vs.f->f_data
		    && frm->vs.f->f_data->frame) {
			/* del_from_list(frm); */
			request_frameset(ses, frm->vs.f->f_data->frame_desc);
#if 0
			destroy_vs(&frm->vs);
			mem_free(frm->name);
			mem_free(frm);
#endif
			mem_free(url);
			return;
		}
		goto found;
	}

	url = stracpy(uurl);
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
	if (*url) {
		request_additional_file(ses, url, PRI_FRAME);
	}

	mem_free(url);
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

void load_frames(struct session *ses, struct f_data_c *fd)
{
	struct f_data *ff = fd->f_data;
	if (!ff || !ff->frame) return;
	request_frameset(ses, ff->frame_desc);
}

void display_timer(struct session *ses)
{
	ttime t = get_time();
	html_interpret(ses);
	draw_formatted(ses);
	t = (get_time() - t) * DISPLAY_TIME;
	if (t < DISPLAY_TIME_MIN) t = DISPLAY_TIME_MIN;
	ses->display_timer = install_timer(t, (void (*)(void *))display_timer, ses);
	load_frames(ses, ses->screen);
	process_file_requests(ses);
}

struct list_head questions_queue = {&questions_queue, &questions_queue};

struct questions_entry {
	struct questions_entry *next;
	struct questions_entry *prev;
	void (*callback)(struct session *);
};

void check_questions_queue(struct session *ses)
{
	struct questions_entry *q;
	while (!list_empty(questions_queue)) {
		q = questions_queue.next;
		q->callback(ses);
		del_from_list(q);
		mem_free(q);
	}
}

void add_questions_entry(void *callback)
{
	struct questions_entry *q;
	if (!(q = mem_alloc(sizeof(struct questions_entry)))) return;
	q->callback = callback;
	add_to_list(questions_queue, q);
}

void end_load(struct status *stat, struct session *ses)
{
	int d;

	if (!ses->wtd) {
		internal("end_load: !ses->wtd");
		return;
	}
	d = do_move(ses, &stat);
	if (!stat) return;
	if (d == 1) {
		stat->end = (void (*)(struct status *, void *))doc_end_load;
		display_timer(ses);
	}
	if (stat->state < 0) {
		if (d != 2 && ses->wtd) {
			free_wtd(ses);
		}
		if (d == 1) doc_end_load(stat, ses);
	}
	if (stat->state < 0 && stat->state != S_OK && d != 2) {
		print_error_dialog(ses, stat, TEXT(T_ERROR));
	}
	check_questions_queue(ses);
	print_screen_status(ses);
}

#ifdef HAVE_SCRIPTING
void maybe_pre_format_html(struct status *stat, struct session *ses)
{
	struct fragment *fr;
	unsigned char *s;
	int len;

	if (stat->ce && !stat->ce->done_pre_format_html_hook) {
		defrag_entry(stat->ce);
		fr = stat->ce->frag.next;
		len = fr->length;
		s = script_hook_pre_format_html(ses, stat->ce->url, fr->data, &len);
		if (s) {
			add_fragment(stat->ce, 0, s, len);
			truncate_entry(stat->ce, len, 1);
			mem_free(s);
		}
		stat->ce->done_pre_format_html_hook = 1;
	}
}
#endif

void doc_end_load(struct status *stat, struct session *ses)
{
	if (stat->state < 0) {
#ifdef HAVE_SCRIPTING
		maybe_pre_format_html(stat, ses);
#endif
		if (ses->display_timer != -1) kill_timer(ses->display_timer), ses->display_timer = -1;
		html_interpret(ses);
		draw_formatted(ses);
		load_frames(ses, ses->screen);
		process_file_requests(ses);
		if (stat->state != S_OK) print_error_dialog(ses, stat, TEXT(T_ERROR));
	} else if (ses->display_timer == -1) display_timer(ses);
	check_questions_queue(ses);
	print_screen_status(ses);

	add_global_history_item(cur_loc(ses)->vs.url,
				ses->screen->f_data->title, time(NULL));
}

void
file_end_load(struct status *stat, struct file_to_load *ftl)
{
	if (ftl->stat.ce) {
		if (ftl->ce) ftl->ce->refcount--;
		(ftl->ce = ftl->stat.ce)->refcount++;
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

	foreach(ftl, ses->more_files) {
		if (!strcmp(ftl->url, url)) {
			if (ftl->pri > pri) {
				ftl->pri = pri;
				change_connection(&ftl->stat, &ftl->stat, pri);
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
		change_connection(stat, NULL, PRI_CANCEL);
		return NULL;
	}

	ftl->req_sent = 1;
	ftl->ce = stat->ce;

	change_connection(stat, &ftl->stat, pri);

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
		foreach(ftl, ses->more_files) {
			if (ftl->req_sent)
				continue;

			ftl->req_sent = 1;
			load_url(ftl->url, (fd && fd->f_data) ? fd->f_data->url
							      : NULL,
				 &ftl->stat, ftl->pri, NC_CACHE);
			more = 1;
		}
	}

	stop_recursion = 0;
}

struct session *create_session(struct window *win)
{
	struct terminal *term = win->term;
	struct session *ses;
	if ((ses = mem_alloc(sizeof(struct session)))) {
		memset(ses, 0, sizeof(struct session));
		create_history(ses);
		init_list(ses->scrn_frames);
		init_list(ses->more_files);
		ses->term = term;
		ses->win = win;
		ses->id = session_id++;
		ses->screen = NULL;
		ses->wtd = WTD_NO;
		ses->display_timer = -1;
		ses->loading_url = NULL;
		ses->goto_position = NULL;
		memcpy(&ses->ds, &dds, sizeof(struct document_setup));
		add_to_list(sessions, ses);
	}
	if (first_use) {
		first_use = 0;
		msg_box(term, NULL,
			TEXT(T_WELCOME), AL_CENTER | AL_EXTD_TEXT,
			TEXT(T_WELCOME_TO_LINKS), "\n\n",
			TEXT(T_BASIC_HELP), NULL,
			NULL, 1,
			TEXT(T_OK), NULL, B_ENTER | B_ESC);
	}
	return ses;
}

void copy_session(struct session *old, struct session *new)
{
	if (have_location(old)) {
		goto_url(new, cur_loc(old)->vs.url);
	}
}

void *create_session_info(int cp, unsigned char *url, int *ll)
{
	int l = strlen(url);
	int *i;
	*ll = 2 * sizeof(int) + l;
	if (!(i = mem_alloc(2 * sizeof(int) + l))) return NULL;
	i[0] = cp;
	i[1] = l;
	memcpy(i + 2, url, l);
	return i;
}

/* This is _NOT_ for what do you think it's for! We use this to make URL
 * shell-safe, nothing more. */
unsigned char *encode_url(unsigned char *url)
{
	unsigned char *u = init_str();
	int l = 0;
	for (; *url; url++) {
		if (is_safe_in_shell(*url) && *url != '+') add_chr_to_str(&u, &l, *url);
		else add_chr_to_str(&u, &l, '+'), add_chr_to_str(&u, &l, hx(*url >> 4)), add_chr_to_str(&u, &l, hx(*url & 0xf));
	}
	return u;
}

/* This is _NOT_ for what do you think it's for! We use this to recover from
 * making URL shell-safe, nothing more. */
unsigned char *decode_url(unsigned char *url)
{
	unsigned char *u = init_str();
	int l = 0;
	for (; *url; url++) {
		if (*url != '+' || unhx(url[1]) == -1 || unhx(url[2]) == -1) add_chr_to_str(&u, &l, *url);
		else add_chr_to_str(&u, &l, (unhx(url[1]) << 4) + unhx(url[2])), url += 2;
	}
	return u;
}

int read_session_info(int fd, struct session *ses, void *data, int len)
{
	unsigned char *h;
	int cpfrom, sz;
	struct session *s;
	if (len < 2 * sizeof(int)) return -1;
	cpfrom = *(int *)data;
	sz = *((int *)data + 1);
	foreach(s, sessions) if (s->id == cpfrom) {
		copy_session(s, ses);
		break;
	}
	if (sz) {
		char *u, *uu;
		if (len < 2 * sizeof(int) + sz) return 0;
		if ((u = mem_alloc(sz + 1))) {
			memcpy(u, (int *)data + 2, sz);
			u[sz] = 0;
			uu = decode_url(u);
			goto_url(ses, uu);
			mem_free(u);
			mem_free(uu);
		}
	} else {
		h = getenv("WWW_HOME");
		if (!h || !*h)
			h = WWW_HOME_URL;
		if (!h || !*h) {
#if 0
			/* I can't do it here - it doesn't work everytime and
			 * it leaks. --pasky */
			if (startup_goto_dialog)
				dialog_goto_url(ses, "");
#endif
		} else {
			goto_url(ses, h);
		}
	}
	return 0;
}

void abort_preloading(struct session *ses)
{
	if (ses->wtd) {
		change_connection(&ses->loading, NULL, PRI_CANCEL);
		free_wtd(ses);
	}
}

void abort_loading(struct session *ses)
{
	struct location *l = cur_loc(ses);
	if (have_location(ses)) {
		if (l->stat.state >= 0)
			change_connection(&l->stat, NULL, PRI_CANCEL);
		abort_files_load(ses);
	}
	abort_preloading(ses);
}

void destroy_session(struct session *ses)
{
	struct f_data_c *fdc;
	if (!ses) return;
	destroy_downloads(ses);
	abort_loading(ses);
	free_files(ses);
	if (ses->screen) detach_formatted(ses->screen), mem_free(ses->screen);
	foreach(fdc, ses->scrn_frames) detach_formatted(fdc);
	free_list(ses->scrn_frames);
	destroy_history(ses);
	if (ses->loading_url) mem_free(ses->loading_url);
	if (ses->display_timer != -1) kill_timer(ses->display_timer);
	if (ses->goto_position) mem_free(ses->goto_position);
	if (ses->imgmap_href_base) mem_free(ses->imgmap_href_base);
	if (ses->imgmap_target_base) mem_free(ses->imgmap_target_base);
	if (ses->tq_ce) ses->tq_ce->refcount--;
	if (ses->tq_url) {
		change_connection(&ses->tq, NULL, PRI_CANCEL);
		mem_free(ses->tq_url);
	}
	if (ses->tq_goto_position) mem_free(ses->tq_goto_position);
	if (ses->tq_prog) mem_free(ses->tq_prog);
	if (ses->dn_url) mem_free(ses->dn_url);
	if (ses->ref_url) mem_free(ses->ref_url),ses->ref_url=NULL;
	if (ses->search_word) mem_free(ses->search_word);
	if (ses->last_search_word) mem_free(ses->last_search_word);
	del_from_list(ses);
	/*mem_free(ses);*/
}

void destroy_all_sessions()
{
	/*while (!list_empty(sessions)) destroy_session(sessions.next);*/
}

void reload(struct session *ses, enum cache_mode cache_mode)
{
	struct location *l;
	struct f_data_c *fd = current_frame(ses);
	
	abort_loading(ses);
	if (cache_mode == -1) cache_mode = ++ses->reloadlevel;
	else ses->reloadlevel = cache_mode;
	l = cur_loc(ses);
	if (have_location(ses)) {
		struct file_to_load *ftl;

		l->stat.data = ses;
		l->stat.end = (void *)doc_end_load;
		load_url(l->vs.url, ses->ref_url, &l->stat, PRI_MAIN, cache_mode);
		foreach(ftl, ses->more_files) {
			if (ftl->req_sent && ftl->stat.state >= 0) continue;
			ftl->stat.data = ftl;
			ftl->stat.end = (void *)file_end_load;
			load_url(ftl->url, fd?fd->f_data?fd->f_data->url:NULL:NULL, &ftl->stat, PRI_FRAME, cache_mode);
		}
	}
}

#if 0
void ses_load_notify(struct status *stat, struct session *ses)
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
		  enum session_wtd wtd, enum cache_mode cache_mode)
{
	unsigned char *u;
	unsigned char *pos;
	void (*fn)(struct session *, unsigned char *);
	struct f_data_c *fd = current_frame(ses);
	int l = 0;

	fn = get_external_protocol_function(url);
	if (fn) {
		fn(ses, url);
		goto end;
	}

	ses->reloadlevel = cache_mode;

	u = translate_url(url, ses->term->cwd);
	if (!u) {
		struct status stat = { NULL, NULL, NULL, NULL, S_BAD_URL,
				       PRI_CANCEL, 0, NULL, NULL };

		print_error_dialog(ses, &stat, TEXT(T_ERROR));
		goto end;
	}
	pos = extract_position(u);

	if (ses->wtd == wtd) {
		if (!strcmp(ses->loading_url, u)) {
			/* We're already loading the URL. */
			mem_free(u);

			if (ses->goto_position)
				mem_free(ses->goto_position);
			ses->goto_position = pos;

			goto end;
		}
	}

	abort_loading(ses);
	if (ses->ref_url) mem_free(ses->ref_url), ses->ref_url=NULL;

	if (fd && fd->f_data && fd->f_data->url) {
 		ses->ref_url = init_str();
		add_to_str(&ses->ref_url, &l, fd->f_data->url);
	}

	ses_goto(ses, u, target, PRI_MAIN, cache_mode, wtd, pos, end_load, 0);

	/* abort_loading(ses); */

end:
	clean_unhistory(ses);
}

void
goto_url_w(struct session *ses, unsigned char *url, unsigned char *target,
	   enum session_wtd wtd, enum cache_mode cache_mode)
{
#ifdef HAVE_SCRIPTING
	url = script_hook_follow_url(ses, url);
	if (url) {
		really_goto_url_w(ses, url, target, wtd, cache_mode);
		mem_free(url);
	}
#else
	really_goto_url_w(ses, url, target, wtd, cache_mode);
#endif
}

void
goto_url_frame_reload(struct session *ses, unsigned char *url,
		      unsigned char *target)
{
	goto_url_w(ses, url, target, WTD_FORWARD, NC_RELOAD);
}

void
goto_url_frame(struct session *ses, unsigned char *url,
	       unsigned char *target)
{
	goto_url_w(ses, url, target, WTD_FORWARD, NC_CACHE);
}

void goto_url(struct session *ses, unsigned char *url)
{
	goto_url_w(ses, url, NULL, WTD_FORWARD, NC_CACHE);
}

/* TODO: Should there be goto_imgmap_reload() ? */

void goto_imgmap(struct session *ses, unsigned char *url, unsigned char *href, unsigned char *target)
{
	if (ses->imgmap_href_base) mem_free(ses->imgmap_href_base);
	ses->imgmap_href_base = href;
	if (ses->imgmap_target_base) mem_free(ses->imgmap_target_base);
	ses->imgmap_target_base = target;
	goto_url_w(ses, url, target, WTD_IMGMAP, NC_CACHE);
}

struct frame *ses_find_frame(struct session *ses, unsigned char *name)
{
	struct location *l = cur_loc(ses);
	struct frame *frm;
	
	if (!have_location(ses)) {
		internal("ses_request_frame: no location yet");
		return NULL;
	}
	foreachback(frm, l->frames) if (!strcasecmp(frm->name, name)) return frm;
	/*internal("ses_find_frame: frame not found");*/
	return NULL;
}

struct frame *ses_change_frame_url(struct session *ses, unsigned char *name, unsigned char *url)
{
	struct location *l = cur_loc(ses);
	struct frame *frm;
	
	if (!have_location(ses)) {
		internal("ses_change_frame_url: no location yet");
		return NULL;
	}
	foreachback(frm, l->frames) if (!strcasecmp(frm->name, name)) {
		if (strlen(url) > strlen(frm->vs.url)) {
			struct f_data_c *fd;
			struct frame *nf = frm;
			if (!(nf = mem_realloc(frm, sizeof(struct frame) + strlen(url) + 1))) return NULL;
			nf->prev->next = nf->next->prev = nf;
			foreach(fd, ses->scrn_frames) {
				if (fd->vs == &frm->vs) fd->vs = &nf->vs;
			}
			frm = nf;
		}
		strcpy(frm->vs.url, url);
		return frm;
	}
	return NULL;

}

void win_func(struct window *win, struct event *ev, int fw)
{
	struct session *ses = win->data;
	switch (ev->ev) {
		case EV_ABORT:
			destroy_session(ses);
			break;
		case EV_INIT:
			if (!(ses = win->data = create_session(win)) ||
			    read_session_info(win->term->fdin, ses, (char *)ev->b + sizeof(int), *(int *)ev->b)) {
				destroy_terminal(win->term);
				return;
			}
		case EV_RESIZE:
			html_interpret(ses);
			draw_formatted(ses);
			load_frames(ses, ses->screen);
			process_file_requests(ses);
			print_screen_status(ses);
			break;
		case EV_REDRAW:
			draw_formatted(ses);
			print_screen_status(ses);
			break;
		case EV_KBD:
		case EV_MOUSE:
			send_event(ses, ev);
			break;
		default:
			error("ERROR: unknown event");
	}
}

/*
 * Gets the url being viewed by this session. Writes it into str.
 * A maximum of str_size bytes (including null) will be written.
 */
unsigned char *get_current_url(struct session *ses, unsigned char *str, size_t str_size) {
	unsigned char *here, *end_of_url;
	size_t url_len = 0;

	/* Not looking at anything */
	if (!have_location(ses))
		return NULL;

	here = cur_loc(ses)->vs.url;

	/* Find the length of the url */
	if ((end_of_url = strchr(here, POST_CHAR))) {
		url_len = (size_t)(end_of_url - (unsigned char *)here);
	} else {
		url_len = strlen(here);
	}

	/* Ensure that the url size is not greater than str_size. We can't just
	 * happily strncpy(str, here, str_size) because we have to stop at
	 * POST_CHAR, not only at NULL. */
	if (url_len >= str_size)
			url_len = str_size - 1;

	safe_strncpy(str, here, url_len + 1);

	return str;
}


/*
 * Gets the title of the page being viewed by this session. Writes it into str.
 * A maximum of str_size bytes (including null) will be written.
 */
unsigned char *get_current_title(struct session *ses, unsigned char *str, size_t str_size) {
	struct f_data_c *fd;
	fd = (struct f_data_c *)current_frame(ses);

	/* Ensure that the title is defined */
	if (!fd)
		return NULL;

	return safe_strncpy(str, fd->f_data->title, str_size);
}

/*
 * Gets the url of the link currently selected. Writes it into str.
 * A maximum of str_size bytes (including null) will be written.
 */
unsigned char *get_current_link_url(struct session *ses, unsigned char *str, size_t str_size) {
	struct f_data_c *fd;
    struct link *l;

	fd = (struct f_data_c *)current_frame(ses);
	/* What the hell is an 'fd'? */
	if (!fd)
		return NULL;

	/* Nothing selected? */
    if (fd->vs->current_link == -1)
		return NULL;

    l = &fd->f_data->links[fd->vs->current_link];
	/* Only write a link */
    if (l->type != L_LINK)
		return NULL;

	return safe_strncpy(str, l->where ? l->where : l->where_img, str_size);
}

