/* Visited URL history managment - NOT goto_url_dialog history! */
/* $Id: history.c,v 1.33 2003/10/23 22:18:26 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "config/options.h"
#include "sched/connection.h"
#include "sched/history.h"
#include "sched/location.h"
#include "sched/session.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/view.h"

/* The history itself is stored in struct session as field history,
 * surprisingly. It's a list containing all locations visited in the current
 * session, including the one being visited just now! So the location on the
 * top of the list is the current location.
 *
 * The unhistory is reverse of history, it contains locations which you
 * visited, but then got bored and went back. The fields pushed away from
 * history are moved to unhistory. There's nothing special on the first item of
 * unhistory. */


static inline void
free_history(struct list_head *history)
{
	while (!list_empty(*history)) {
		struct location *loc = history->next;

		destroy_location(loc);
	}
}


void
create_history(struct ses_history *history)
{
	init_list(history->history);
	init_list(history->unhistory);
	history->current = NULL;
}

void
destroy_history(struct ses_history *history)
{
	free_history(&history->history);
	free_history(&history->unhistory);
	history->current = NULL;
}

void
clean_unhistory(struct ses_history *history)
{
	if (get_opt_int("document.history.keep_unhistory")) return;
	free_history(&history->unhistory);
}


/* Common ses_(un)back() backend, doing the actions common for leaving of the
 * current location for movement in the history. */
/* @dir: 1 == forward (unback), -1 == back */
/* Returns < 0 upon error, 0 if we should abort the movement and 1 if we should
 * proceed fearlessly. */
static int
ses_leave_location(struct session *ses, int dir)
{
	free_files(ses);

	if (ses->search_word) {
		mem_free(ses->search_word);
		ses->search_word = NULL;
	}

	if (!have_location(ses))
		return 0;

	return 1;
}

void
ses_back(struct session *ses)
{
	struct location *loc;

	if (ses_leave_location(ses, -1) < 1)
		return;

	/* This is the current location. */
	loc = cur_loc(ses);
    	del_from_history(&ses->history, loc);

	add_to_list(ses->history.unhistory, loc);

	if (!have_location(ses)) return;

	/* This was the previous location (where we came back now). */
	loc = cur_loc(ses);

	if (!strcmp(loc->vs.url, ses->loading_url)) return;

	/* Remake that location. */
    	del_from_history(&ses->history, loc);
	destroy_location(loc);
	ses_forward(ses);
}

void
ses_unback(struct session *ses)
{
	struct location *loc;

	if (ses_leave_location(ses, 1) < 1)
		return;

	if (list_empty(ses->history.unhistory))
		return;

	loc = ses->history.unhistory.next;

    	del_from_history(&ses->history, loc);
	/* Save it as the current location! */
	add_to_list(ses->history.history, loc);

	if (!strcmp(loc->vs.url, ses->loading_url)) return;

	/* Remake that location. */
    	del_from_history(&ses->history, loc);
	destroy_location(loc);
	ses_forward(ses);
}


/* Common part of go_(un)back(). */
/* @dir: 1 == forward (unback), -1 == back */
/* Returns < 0 upon error, 0 if we should abort the movement and 1 if we should
 * proceed fearlessly. */
static int
go_away(struct session *ses, int dir)
{
	struct document_view *doc_view = current_frame(ses);
	struct list_head *history = (dir == 1	? &ses->history.unhistory
						: &ses->history.history);

	ses->reloadlevel = NC_CACHE;

	if (ses->task) {
		abort_loading(ses, 0);
		print_screen_status(ses);
		reload(ses, NC_CACHE);
		return 0;
	}

	if (!have_location(ses)
	    || (dir == -1 && history->prev == history->next)
	    || (dir == 1 && list_empty(*history))) {
		/* There's no history, at most only the current location. */
		return 0;
	}

	abort_loading(ses, 0);

	if (ses->ref_url) {
		mem_free(ses->ref_url);
		ses->ref_url = NULL;
	}

	if (doc_view && doc_view->document && doc_view->document->url)
		ses->ref_url = stracpy(doc_view->document->url);

	return 1;
}

void
go_back(struct session *ses)
{
	unsigned char *url;
	struct location *loc;

	if (go_away(ses, -1) < 1)
		return;

	loc = cur_loc(ses)->next;
	url = memacpy(loc->vs.url, loc->vs.url_len);
	if (!url) return;

	ses_goto(ses, url, NULL, PRI_MAIN, NC_ALWAYS_CACHE, TASK_BACK, NULL,
		 end_load, 0);
}

void
go_unback(struct session *ses)
{
	unsigned char *url;
	struct location *loc;

	if (go_away(ses, 1) < 1)
		return;

	loc = (struct location *) ses->history.unhistory.next;
	url = memacpy(loc->vs.url, loc->vs.url_len);
	if (!url) return;

	ses_goto(ses, url, NULL, PRI_MAIN, NC_ALWAYS_CACHE, TASK_UNBACK, NULL,
		 end_load, 1);
}
