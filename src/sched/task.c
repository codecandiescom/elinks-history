/* Sessions task management */
/* $Id: task.c,v 1.72 2004/04/14 00:11:51 jonas Exp $ */

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
#include "protocol/uri.h"
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

	if (ses->loading_uri) {
		done_uri(ses->loading_uri);
		ses->loading_uri = NULL;
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
	struct uri *uri;
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
	ses->loading_uri = task->uri; /* XXX: Make the session inherit the URI. */

	ses->task.type = task->type;
	ses->task.target_frame = task->target_frame;
	ses->task.target_location = task->target_location;

	load_uri(ses->loading_uri, ses->referrer,
		 &ses->loading, task->pri, task->cache_mode, -1);
}

static void
post_no(struct task *task)
{
	reload(task->ses, CACHE_MODE_NORMAL);
	done_uri(task->uri);
}

void
ses_goto(struct session *ses, struct uri *uri, unsigned char *target_frame,
	 struct location *target_location,
	 int pri, enum cache_mode cache_mode, enum task_type task_type,
	 unsigned char *pos,
	 void (*fn)(struct download *, struct session *),
	 int redir)
{
	struct task *task = mem_alloc(sizeof(struct task));
	unsigned char *m1, *m2;
	struct cache_entry *cached;

	if (ses->doc_view
	    && ses->doc_view->document
	    && ses->doc_view->document->refresh) {
		kill_document_refresh(ses->doc_view->document->refresh);
	}

	assertm(!ses->loading_uri, "Buggy URI reference counting");

	/* Do it here because it might be ses->goto_position being passed */
	pos = null_or_stracpy(pos);

	if (!task
	    || !uri->post
	    || !get_opt_int("document.browse.forms.confirm_submit")
	    || (cache_mode == CACHE_MODE_ALWAYS
		&& (cached = find_in_cache(uri))
		&& !cached->incomplete)) {

		if (task) mem_free(task);

		if (ses->goto_position) mem_free(ses->goto_position);
		ses->goto_position = pos;

		ses->loading.end = (void (*)(struct download *, void *)) fn;
		ses->loading.data = ses;
		ses->loading_uri = get_uri_reference(uri);

		ses->task.type = task_type;
		ses->task.target_frame = target_frame;
		ses->task.target_location = target_location;

		load_uri(ses->loading_uri, ses->referrer, &ses->loading,
			 pri, cache_mode, -1);

		return;
	}

	task->ses = ses;
	task->uri = get_uri_reference(uri);
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

	m2 = get_uri_string(uri, ~URI_POST);
	msg_box(ses->tab->term, getml(task, task->pos, NULL), MSGBOX_FREE_TEXT,
		N_("Warning"), AL_CENTER,
		msg_text(ses->tab->term, m1, m2),
		task, 2,
		N_("Yes"), post_yes, B_ENTER,
		N_("No"), post_no, B_ESC);
	if (m2) mem_free(m2);
}


struct view_state *
ses_forward(struct session *ses, int loaded_in_frame)
{
	struct location *loc = NULL;
	struct view_state *vs;

	if (!loaded_in_frame) {
		free_files(ses);

		if (ses->search_word) {
			mem_free(ses->search_word);
			ses->search_word = NULL;
		}
	}

x:
	if (!loaded_in_frame) {
		loc = mem_calloc(1, sizeof(struct location));
		if (!loc) return NULL;
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
		frame = ses_change_frame_uri(ses, ses->task.target_frame,
					     ses->loading_uri);

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
			init_vs(vs, ses->loading_uri, vs->plain);
			if (ses->goto_position) {
				frame->vs.goto_position = ses->goto_position;
				ses->goto_position = NULL;
			}
		}
	} else {
		init_list(loc->frames);
		vs = &loc->vs;
		init_vs(vs, ses->loading_uri, vs->plain);
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
	struct cache_entry *cached = find_in_cache(ses->loading_uri);
	struct fragment *fr;
	struct memory_list *ml;
	struct menu_item *menu;

	if (!cached) {
		INTERNAL("can't find cache entry");
		return;
	}
	defrag_entry(cached);
	fr = cached->frag.next;
	if ((void *)fr == &cached->frag) return;

	if (get_image_map(cached->head, fr->data, fr->data + fr->length,
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
	struct cache_entry *cached;

	assert(stat && *stat);
	assertm(ses->loading_uri, "no ses->loading_uri");
	if_assert_failed return 0;

	if (ses->loading_uri->protocol == PROTOCOL_UNKNOWN)
		return 0;

	if (ses->task.type == TASK_IMGMAP && (*stat)->state >= 0)
		return 0;

	cached = (*stat)->cached;
	if (!cached) return 0;

	if (cached->redirect && ses->redirect_cnt++ < MAX_REDIRECTS) {
		enum task_type task = ses->task.type;

		if (task == TASK_HISTORY && !have_location(ses))
			goto b;

		assertm(cached->uri == ses->loading_uri, "Redirecting using bad base URI");

		if (cached->redirect->protocol == PROTOCOL_UNKNOWN)
			return 0;

		abort_loading(ses, 0);
		if (have_location(ses))
			*stat = &cur_loc(ses)->download;
		else
			*stat = NULL;

		set_session_referrer(ses, get_cache_uri(cached));

		switch (task) {
		case TASK_NONE:
			break;
		case TASK_FORWARD:
		{
			protocol_external_handler *fn;
			struct uri *uri = cached->redirect; 

			fn = get_protocol_external_handler(uri->protocol);
			if (fn) {
				fn(ses, uri);
				*stat = NULL;
				return 0;
			}
		}
			/* Fall through. */
		case TASK_IMGMAP:
			ses_goto(ses, cached->redirect, ses->task.target_frame, NULL,
				 PRI_MAIN, CACHE_MODE_NORMAL, task,
				 ses->goto_position, end_load, 1);
			return 2;
		case TASK_HISTORY:
			ses_goto(ses, cached->redirect, NULL, ses->task.target_location,
				 PRI_MAIN, CACHE_MODE_NORMAL, TASK_RELOAD,
				 NULL, end_load, 1);
			return 2;
		case TASK_RELOAD:
			ses_goto(ses, cached->redirect, NULL, NULL,
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
			if (ses_chktype(ses, &ses->loading, cached, 0)) {
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

	if (is_in_result_state(stat->state)) {
		if (ses->task.type) free_task(ses);
		if (d == 1) doc_end_load(stat, ses);
	}

	if (is_in_result_state(stat->state) && stat->state != S_OK) {
		print_error_dialog(ses, stat->state, stat->pri);
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
	struct uri *referrer = NULL;
	unsigned char *pos = NULL;
	struct uri *uri = get_translated_uri(url, ses->tab->term->cwd, &pos);
	protocol_external_handler *external_handler;

	if (!uri) {
		if (pos) mem_free(pos);
		print_error_dialog(ses, S_BAD_URL, PRI_CANCEL);
		return;
	}

	external_handler = get_protocol_external_handler(uri->protocol);
	if (external_handler) {
		external_handler(ses, uri);
		if (pos) mem_free(pos);
		done_uri(uri);
		return;
	}

	ses->reloadlevel = cache_mode;

	if (ses->task.type == task) {
		if (ses->loading_uri == uri) {
			/* We're already loading the URL. */
			done_uri(uri);

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
			referrer = doc_view->document->uri;
	}

	set_session_referrer(ses, referrer);

	ses_goto(ses, uri, target, NULL,
		 PRI_MAIN, cache_mode, task,
		 pos, end_load, 0);
	done_uri(uri);
	if (pos) mem_free(pos);
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
