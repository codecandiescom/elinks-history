/* Visited URL history managment - NOT goto_url_dialog history! */
/* $Id: history.c,v 1.54 2003/11/12 06:03:16 witekfl Exp $ */

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

	while (history->current->next != (struct location *) &history->history) {
		struct location *loc = history->current->next;

		del_from_list(loc);
		destroy_location(loc);
	}
}


void
ses_history_move(struct session *ses)
{
	struct location *loc;

	/* Prepare. */

	free_files(ses);

	if (ses->search_word) {
		mem_free(ses->search_word);
		ses->search_word = NULL;
	}

	/* Does it make sense? */

	if (!have_location(ses) || !ses->task_target_location)
		return;

	if (ses->task_target_location
	    == (struct location *) &ses->history.history)
		return;

	/* Move. */

	ses->history.current = ses->task_target_location;

	loc = cur_loc(ses);

	if (!strcmp(loc->vs.url, ses->loading_url))
		return;

	/* Remake that location. */

    	del_from_history(&ses->history, loc);
	destroy_location(loc);
	ses_forward(ses, NULL);

	/* Maybe trash the unhistory. */

	if (get_opt_bool("document.history.keep_unhistory"))
		clean_unhistory(&ses->history);
}


void
go_history(struct session *ses, struct location *loc)
{
	unsigned char *url;

	ses->reloadlevel = CACHE_MODE_NORMAL;

	if (ses->task) {
		abort_loading(ses, 0);
		print_screen_status(ses);
		reload(ses, CACHE_MODE_NORMAL);
		return;
	}

	if (!have_location(ses)
	    || loc == (struct location *) &ses->history.history) {
		/* There's no history, at most only the current location. */
		return;
	}

	abort_loading(ses, 0);

	set_referrer(ses, NULL);

	url = memacpy(loc->vs.url, loc->vs.url_len);
	if (!url) return;

	ses_goto(ses, url, NULL, loc,
		 PRI_MAIN, CACHE_MODE_ALWAYS, TASK_HISTORY,
		 NULL, end_load, 0);
}
