/* Visited URL history managment - NOT goto_url_dialog history! */
/* $Id: history.c,v 1.39 2003/10/24 00:33:00 pasky Exp $ */

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


static inline void
free_history(struct list_head *history)
{
	while (!list_empty(*history)) {
		struct location *loc = history->next;

		del_from_list(loc);
		destroy_location(loc);
	}
}


void
create_history(struct ses_history *history)
{
	init_list(history->history);
	history->current = NULL;
}

void
destroy_history(struct ses_history *history)
{
	free_history(&history->history);
	history->current = NULL;
}

void
clean_unhistory(struct ses_history *history)
{
	if (!history->current) return;

	while (history->current->next != history) {
		struct location *loc = history->current->next;

		del_from_list(loc);
		destroy_location(loc);
	}
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
	if (loc->prev == (struct location *) &ses->history.history) return;
	ses->history.current = ses->history.current->prev;

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

	/* This is the current location. */

	loc = cur_loc(ses);
	if (loc->next == (struct location *) &ses->history.history) return;
	ses->history.current = ses->history.current->next;

	/* This will be the next location (where we came back now). */

	loc = cur_loc(ses);

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
go_away(struct session *ses, struct location *loc, int dir)
{
	ses->reloadlevel = NC_CACHE;

	if (ses->task) {
		abort_loading(ses, 0);
		print_screen_status(ses);
		reload(ses, NC_CACHE);
		return 0;
	}

	if (!have_location(ses)
	    || loc == (struct location *) &ses->history.history) {
		/* There's no history, at most only the current location. */
		return 0;
	}

	abort_loading(ses, 0);

	if (ses->ref_url) {
		mem_free(ses->ref_url);
		ses->ref_url = NULL;
	}

	return 1;
}

void
go_back(struct session *ses, struct location *loc)
{
	unsigned char *url;

	if (go_away(ses, loc, -1) < 1)
		return;

	url = memacpy(loc->vs.url, loc->vs.url_len);
	if (!url) return;

	ses_goto(ses, url, NULL, PRI_MAIN, NC_ALWAYS_CACHE, TASK_BACK, NULL,
		 end_load, 0);
}

void
go_unback(struct session *ses, struct location *loc)
{
	unsigned char *url;

	if (go_away(ses, loc, 1) < 1)
		return;

	url = memacpy(loc->vs.url, loc->vs.url_len);
	if (!url) return;

	ses_goto(ses, url, NULL, PRI_MAIN, NC_ALWAYS_CACHE, TASK_UNBACK, NULL,
		 end_load, 1);
}
