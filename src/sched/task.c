/* Sessions task management */
/* $Id: task.c,v 1.28 2004/03/23 20:44:44 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/menu.h"
#include "bfu/msgbox.h"
#include "bfu/style.h"
#include "cache/cache.h"
#include "dialogs/status.h"
#include "document/document.h"
#include "document/html/parser.h"
#include "document/refresh.h"
#include "document/view.h"
#include "intl/gettext/libintl.h"
#include "lowlevel/select.h"
#include "protocol/protocol.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "sched/download.h"
#include "sched/event.h"
#include "sched/session.h"
#include "sched/task.h"
#include "viewer/text/view.h"


static void
free_task(struct session *ses)
{
	assertm(ses->task.type, "Session has no task");
	if_assert_failed return;

	if (ses->goto_position) {
		mem_free(ses->goto_position);
		ses->goto_position = NULL;
	}

	if (ses->loading_url) {
		mem_free(ses->loading_url);
		ses->loading_url = NULL;
	}
	ses->task.type = TASK_NONE;
}

void
abort_preloading(struct session *ses, int interrupt)
{
	if (!ses->task.type) return;

	change_connection(&ses->loading, NULL, PRI_CANCEL, interrupt);
	free_task(ses);
}


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

	ses->goto_position = null_or_stracpy(task->pos);
	ses->loading.end = (void (*)(struct download *, void *)) task->fn;
	ses->loading.data = task->ses;
	ses->loading_url = stracpy(task->url);
	ses->task.type = task->type;
	ses->task.target_frame = task->target_frame;
	ses->task.target_location = task->target_location;

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
	unsigned char *post_char_pos = post_data_start(url);

	if (ses->doc_view
	    && ses->doc_view->document
	    && ses->doc_view->document->refresh) {
		kill_document_refresh(ses->doc_view->document->refresh);
	}

	if (!task
	    || !get_opt_int("document.browse.forms.confirm_submit")
	    || !post_char_pos
	    || (cache_mode == CACHE_MODE_ALWAYS
		&& (e = find_in_cache(url))
		&& !e->incomplete)) {

		if (task) mem_free(task);

		if (ses->goto_position) mem_free(ses->goto_position);
		ses->goto_position = pos;

		ses->loading.end = (void (*)(struct download *, void *)) fn;
		ses->loading.data = ses;
		ses->loading_url = url;
		ses->task.type = task_type;
		ses->task.target_frame = target_frame;
		ses->task.target_location = target_location;

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


struct view_state *
ses_forward(struct session *ses, int loaded_in_frame)
{
	struct location *loc = NULL;
	struct view_state *vs;
	int len;

	if (!loaded_in_frame) {
		free_files(ses);

		if (ses->search_word) {
			mem_free(ses->search_word);
			ses->search_word = NULL;
		}
	}

x:
	if (!loaded_in_frame) {
		/* The new location will either be pointing to the URL
		 * of the current location or the loading URL so make
		 * it big enough. */
		len = strlen(ses->loading_url);
		if (have_location(ses))
			int_lower_bound(&len, cur_loc(ses)->vs.url_len);

		/* struct view_state reserves one byte, so len is sufficient. */
		loc = mem_alloc(sizeof(struct location) + len);
		if (!loc) return NULL;
		memset(loc, 0, sizeof(struct location));
		memcpy(&loc->download, &ses->loading, sizeof(struct download));
	}

	if (ses->task.target_frame && *ses->task.target_frame) {
		struct frame *frame;

		assertm(have_location(ses), "no location yet");
		if_assert_failed return NULL;

		if (!loaded_in_frame) {
			copy_location(loc, cur_loc(ses));
			add_to_history(&ses->history, loc);
		}
		frame = ses_change_frame_url(ses, ses->task.target_frame,
					     ses->loading_url);

		if (!frame) {
			if (!loaded_in_frame) {
				del_from_history(&ses->history, loc);
				destroy_location(loc);
			}
			ses->task.target_frame = NULL;
			goto x;
		}

		vs = &frame->vs;
		if (!loaded_in_frame) {
			destroy_vs(vs);
			init_vs(vs, ses->loading_url, vs->plain);
		}

		if (!loaded_in_frame) {
			if (ses->goto_position) {
				if (frame->vs.goto_position)
					mem_free(frame->vs.goto_position);
				frame->vs.goto_position = ses->goto_position;
				ses->goto_position = NULL;
			}
		}
#if 0
		request_additional_loading_file(ses, ses->loading_url,
						&ses->loading, PRI_FRAME);
#endif
	} else {
		init_list(loc->frames);
		vs = &loc->vs;
		init_vs(vs, ses->loading_url, vs->plain);
		add_to_history(&ses->history, loc);

		if (ses->goto_position) {
			loc->vs.goto_position = ses->goto_position;
			ses->goto_position = NULL;
		}
	}

	ses->status.visited = 0;

	/* This is another "branch" in the browsing, so throw away the current
	 * unhistory, we are venturing in another direction! */
	if (ses->task.type == TASK_FORWARD)
		clean_unhistory(&ses->history);
	return vs;
}

static void
ses_imgmap(struct session *ses)
{
	struct cache_entry *ce = find_in_cache(ses->loading_url);
	struct fragment *fr;
	struct memory_list *ml;
	struct menu_item *menu;

	if (!ce) {
		INTERNAL("can't find cache entry");
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

	if (ses->task.type == TASK_IMGMAP && (*stat)->state >= 0)
		return 0;

	ce = (*stat)->ce;
	if (!ce) return 0;

	if (ce->redirect && ses->redirect_cnt++ < MAX_REDIRECTS) {
		unsigned char *u;
		enum task_type task = ses->task.type;

		if (task == TASK_HISTORY && !have_location(ses))
			goto b;

		u = join_urls(ses->loading_url, ce->redirect);
		if (!u) goto b;

		if (!ce->redirect_get &&
		    !get_opt_int("protocol.http.bugs.broken_302_redirect")) {
			unsigned char *p = post_data_start(ses->loading_url);

			if (p) add_to_strn(&u, p);
		}
		/* ^^^^ According to RFC2068 POST must not be redirected to GET, but
			some BUGGY message boards rely on it :-( */

		protocol = known_protocol(u, NULL);
		if (protocol == PROTOCOL_UNKNOWN) return 0;

		abort_loading(ses, 0);
		if (have_location(ses))
			*stat = &cur_loc(ses)->download;
		else
			*stat = NULL;

		set_referrer(ses, get_cache_uri(ce));

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

			ses_goto(ses, u, ses->task.target_frame, NULL,
				 PRI_MAIN, CACHE_MODE_NORMAL, task,
				 gp, end_load, 1);
			if (gp) mem_free(gp);
			return 2;
			}
		case TASK_HISTORY:
			ses_goto(ses, u, NULL, ses->task.target_location,
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

	switch (ses->task.type) {
		case TASK_NONE:
			break;
		case TASK_FORWARD:
			if (ses_chktype(ses, &ses->loading, ce, 0)) {
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
			ses->task.target_location = cur_loc(ses)->prev;
			ses_history_move(ses);
			ses_forward(ses, 0);
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

void
end_load(struct download *stat, struct session *ses)
{
	int d;

	assertm(ses->task.type, "end_load: no ses->task");
	if_assert_failed return;

	d = do_move(ses, &stat);
	if (!stat) return;
	if (d == 2) goto end;

	if (d == 1) {
		stat->end = (void (*)(struct download *, void *))doc_end_load;
		display_timer(ses);
	}

	if (stat->state < 0) {
		if (ses->task.type) free_task(ses);
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


static void
do_follow_url(struct session *ses, unsigned char *url, unsigned char *target,
	      enum task_type task, enum cache_mode cache_mode, int do_referrer)
{
	unsigned char *u, *referrer = NULL;
	unsigned char *pos;
	enum protocol protocol = known_protocol(url, NULL);

	if (protocol == PROTOCOL_UNKNOWN) {
		print_unknown_protocol_dialog(ses);
		return;
	}

	if (protocol != PROTOCOL_INVALID) {
		protocol_external_handler *fn =
			get_protocol_external_handler(protocol);

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

	pos = extract_fragment(u);

	if (ses->task.type == task) {
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
#ifdef HAVE_SCRIPTING
	static int follow_url_event_id = EVENT_NONE;

	url = stracpy(url);
	if (!url) return;

	set_event_id(follow_url_event_id, "follow-url");
	trigger_event(follow_url_event_id, &url, ses);
	if (!url) return;
#endif

	if (*url)
		do_follow_url(ses, url, target, task, cache_mode, referrer);

#ifdef HAVE_SCRIPTING
	mem_free(url);
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
map_selected(struct terminal *term, struct link_def *ld, struct session *ses)
{
	goto_url_frame(ses, ld->link, ld->target);
}

void
goto_url(struct session *ses, unsigned char *url)
{
	follow_url(ses, url, NULL, TASK_FORWARD, CACHE_MODE_NORMAL, 0);
}

void
goto_url_with_hook(struct session *ses, unsigned char *url)
{
#if defined(HAVE_SCRIPTING) || defined(CONFIG_URI_REWRITE)
	static int goto_url_event_id = EVENT_NONE;

	url = stracpy(url);
	if (!url) return;

	set_event_id(goto_url_event_id, "goto-url");
	trigger_event(goto_url_event_id, &url, ses);
	if (!url) return;
#endif

	if (*url) goto_url(ses, url);

#if defined(HAVE_SCRIPTING) || defined(CONFIG_URI_REWRITE)
	mem_free(url);
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
