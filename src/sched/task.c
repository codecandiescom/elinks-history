/* Sessions task management */
/* $Id: task.c,v 1.118 2004/06/23 07:38:26 zas Exp $ */

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
	enum cache_mode cache_mode;
	enum task_type type;
	unsigned char *target_frame;
	struct location *target_location;
};

static void
post_yes(struct task *task)
{
	struct session *ses = task->ses;

	abort_preloading(task->ses, 0);

	ses->loading.end = (void (*)(struct download *, void *)) end_load;
	ses->loading.data = task->ses;
	ses->loading_uri = task->uri; /* XXX: Make the session inherit the URI. */

	ses->task.type = task->type;
	ses->task.target_frame = task->target_frame;
	ses->task.target_location = task->target_location;

	load_uri(ses->loading_uri, ses->referrer, &ses->loading,
		 PRI_MAIN, task->cache_mode, -1);
}

static void
post_no(struct task *task)
{
	reload(task->ses, CACHE_MODE_NORMAL);
	done_uri(task->uri);
}

void
ses_goto(struct session *ses, struct uri *uri, unsigned char *target_frame,
	 struct location *target_location, enum cache_mode cache_mode,
	 enum task_type task_type, int redir)
{
	struct task *task = uri->form ? mem_alloc(sizeof(struct task)) : NULL;
	unsigned char *m1, *m2;
	int referrer_incomplete = 0;
	int confirm_submit = uri->form;

	if (ses->doc_view
	    && ses->doc_view->document
	    && ses->doc_view->document->refresh) {
		kill_document_refresh(ses->doc_view->document->refresh);
	}

	assertm(!ses->loading_uri, "Buggy URI reference counting");

	/* Figure out whether to confirm submit or not */

	/* Only confirm submit if we are posting form data */
	/* Note uri->post might be empty here but we are still supposely
	 * posting form data so this should be more correct. */
	if (!task || !uri->form) {
		confirm_submit = 0;

	} else {
		struct cache_entry *cached;

		/* First check if the referring URI was incomplete. It
		 * indicates that the posted form data might be incomplete too.
		 * See bug 460. */
		if (ses->referrer) {
			cached = find_in_cache(ses->referrer);
			referrer_incomplete = (cached && cached->incomplete);
		}

		if (!get_opt_int("document.browse.forms.confirm_submit")
		    && !referrer_incomplete) {
			confirm_submit = 0;

		} else if (get_validated_cache_entry(uri, cache_mode)) {
			confirm_submit = 0;
		}
	}

	if (!confirm_submit) {
		mem_free_if(task);

		ses->loading.end = (void (*)(struct download *, void *)) end_load;
		ses->loading.data = ses;
		ses->loading_uri = get_uri_reference(uri);

		ses->task.type = task_type;
		ses->task.target_frame = target_frame;
		ses->task.target_location = target_location;

		load_uri(ses->loading_uri, ses->referrer, &ses->loading,
			 PRI_MAIN, cache_mode, -1);

		return;
	}

	task->ses = ses;
	task->uri = get_uri_reference(uri);
	task->cache_mode = cache_mode;
	task->type = task_type;
	task->target_frame = target_frame;
	task->target_location = target_location;

	if (redir) {
		m1 = N_("Do you want to follow redirect and post form data "
			"to URL %s?");

	} else if (referrer_incomplete) {
		m1 = N_("The form data you are about to post might be incomplete.\n"
			"Do you want to post to URL %s?");

	} else if (task_type == TASK_FORWARD) {
		m1 = N_("Do you want to post form data to URL %s?");

	} else {
		m1 = N_("Do you want to repost form data to URL %s?");
	}

	m2 = get_uri_string(uri, URI_PUBLIC);
	msg_box(ses->tab->term, getml(task, NULL), MSGBOX_FREE_TEXT,
		N_("Warning"), AL_CENTER,
		msg_text(ses->tab->term, m1, m2),
		task, 2,
		N_("Yes"), post_yes, B_ENTER,
		N_("No"), post_no, B_ESC);
	mem_free_if(m2);
}


struct view_state *
ses_forward(struct session *ses, int loaded_in_frame)
{
	struct location *loc = NULL;
	struct view_state *vs;

	if (!loaded_in_frame) {
		free_files(ses);
		mem_free_set(&ses->search_word, NULL);
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

		frame = ses_find_frame(ses, ses->task.target_frame);
		if (!frame) {
			if (!loaded_in_frame) {
				del_from_history(&ses->history, loc);
				destroy_location(loc);
			}
			ses->task.target_frame = NULL;
			goto x;
		}

		vs = &frame->vs;
		done_uri(vs->uri);
		vs->uri = get_uri_reference(ses->loading_uri);
		if (!loaded_in_frame) {
			destroy_vs(vs);
			init_vs(vs, ses->loading_uri, vs->plain);
		}
	} else {
		init_list(loc->frames);
		vs = &loc->vs;
		init_vs(vs, ses->loading_uri, vs->plain);
		add_to_history(&ses->history, loc);
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
	if ((void *) fr == &cached->frag) return;

	if (get_image_map(cached->head, fr->data, fr->data + fr->length,
			  &menu, &ml, ses->loading_uri, ses->task.target_frame,
			  get_opt_int_tree(ses->tab->term->spec, "charset"),
			  get_opt_int("document.codepage.assume"),
			  get_opt_int("document.codepage.force_assumed")))
		return;

	add_empty_window(ses->tab->term, (void (*)(void *)) freeml, ml);
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

	/* Handling image map needs to scan the source of the loaded document
	 * so all of it has to be available. */
	if (ses->task.type == TASK_IMGMAP && is_in_progress_state((*stat)->state))
		return 0;

	cached = (*stat)->cached;
	if (!cached) return 0;

	if (cached->redirect && ses->redirect_cnt++ < MAX_REDIRECTS) {
		enum task_type task = ses->task.type;

		if (task == TASK_HISTORY && !have_location(ses))
			goto b;

		assertm(compare_uri(cached->uri, ses->loading_uri, URI_BASE),
			"Redirecting using bad base URI");

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
				 CACHE_MODE_NORMAL, task, 1);
			return 2;
		case TASK_HISTORY:
			ses_goto(ses, cached->redirect, NULL, ses->task.target_location,
				 CACHE_MODE_NORMAL, TASK_RELOAD, 1);
			return 2;
		case TASK_RELOAD:
			ses_goto(ses, cached->redirect, NULL, NULL,
				 ses->reloadlevel, TASK_RELOAD, 1);
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

	if (is_in_progress_state((*stat)->state)) {
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
do_follow_url(struct session *ses, struct uri *uri, unsigned char *target,
	      enum task_type task, enum cache_mode cache_mode, int do_referrer)
{
	struct uri *referrer = NULL;
	protocol_external_handler *external_handler;

	if (!uri) {
		print_error_dialog(ses, S_BAD_URL, PRI_CANCEL);
		return;
	}

	external_handler = get_protocol_external_handler(uri->protocol);
	if (external_handler) {
		external_handler(ses, uri);
		return;
	}

	ses->reloadlevel = cache_mode;

	if (ses->task.type == task) {
		if (compare_uri(ses->loading_uri, uri, 0)) {
			/* We're already loading the URL. */
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

	ses_goto(ses, uri, target, NULL, cache_mode, task, 0);
}

static void
follow_url(struct session *ses, struct uri *uri, unsigned char *target,
	   enum task_type task, enum cache_mode cache_mode, int referrer)
{
#ifdef CONFIG_SCRIPTING
	static int follow_url_event_id = EVENT_NONE;
	unsigned char *uristring = get_uri_string(uri, URI_BASE | URI_FRAGMENT);

	if (!uristring) return;

	set_event_id(follow_url_event_id, "follow-url");
	trigger_event(follow_url_event_id, &uristring, ses);

	if (!uristring || !*uristring) {
		mem_free_if(uristring);
		return;
	}

	/* FIXME: Compare if uristring and struri(uri) are equal */
	/* FIXME: When uri->post will no longer be an encoded string (but
	 * hopefully some refcounted object) we will have to asign the post
	 * data object to the translated URI. */
	uri = get_translated_uri(uristring, ses->tab->term->cwd);
	mem_free(uristring);
#endif

	do_follow_url(ses, uri, target, task, cache_mode, referrer);

#ifdef CONFIG_SCRIPTING
	if (uri) done_uri(uri);
#endif
}

void
goto_uri(struct session *ses, struct uri *uri)
{
	follow_url(ses, uri, NULL, TASK_FORWARD, CACHE_MODE_NORMAL, 0);
}

void
goto_uri_frame(struct session *ses, struct uri *uri,
	       unsigned char *target, enum cache_mode cache_mode)
{
	follow_url(ses, uri, target, TASK_FORWARD, cache_mode, 1);
}

void
map_selected(struct terminal *term, struct link_def *ld, struct session *ses)
{
	struct uri *uri = get_uri(ld->link, 0);

	goto_uri_frame(ses, uri, ld->target, CACHE_MODE_NORMAL);
	if (uri) done_uri(uri);
}


void
goto_url(struct session *ses, unsigned char *url)
{
	struct uri *uri = get_uri(url, 0);

	goto_uri(ses, uri);
	if (uri) done_uri(uri);
}

struct uri *
get_hooked_uri(unsigned char *uristring, struct session *ses, unsigned char *cwd)
{
	struct uri *uri;

#if defined(CONFIG_SCRIPTING) || defined(CONFIG_URI_REWRITE)
	static int goto_url_event_id = EVENT_NONE;

	uristring = stracpy(uristring);
	if (!uristring) return NULL;

	set_event_id(goto_url_event_id, "goto-url");
	trigger_event(goto_url_event_id, &uristring, ses);
	if (!uristring) return NULL;
#endif

	uri = *uristring ? get_translated_uri(uristring, cwd) : NULL;

#if defined(CONFIG_SCRIPTING) || defined(CONFIG_URI_REWRITE)
	mem_free(uristring);
#endif
	return uri;
}

void
goto_url_with_hook(struct session *ses, unsigned char *url)
{
	unsigned char *cwd = ses->tab->term->cwd;
	struct uri *uri;

	/* Bail out if passed empty string from goto-url dialog */
	if (!*url) return;

	uri = get_hooked_uri(url, ses, cwd);
	goto_uri(ses, uri);
	if (uri) done_uri(uri);
}

int
goto_url_home(struct session *ses)
{
	unsigned char *homepage = get_opt_str("ui.sessions.homepage");

	if (!*homepage) homepage = getenv("WWW_HOME");
	if (!homepage || !*homepage) homepage = WWW_HOME_URL;

	if (!homepage || !*homepage) return 0;

	goto_url_with_hook(ses, homepage);
	return 1;
}

/* TODO: Should there be goto_imgmap_reload() ? */

void
goto_imgmap(struct session *ses, struct uri *uri, unsigned char *target)
{
	follow_url(ses, uri, target, TASK_IMGMAP, CACHE_MODE_NORMAL, 1);
}
