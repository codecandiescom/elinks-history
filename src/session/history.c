/* Visited URL history managment - NOT goto_url_dialog history! */
/* $Id: history.c,v 1.4 2003/06/08 21:45:27 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "config/options.h"
#include "sched/history.h"
#include "sched/location.h"
#include "sched/sched.h"
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
		loc = history->next;
	}
}


void
create_history(struct session *ses)
{
	init_list(ses->history);
	init_list(ses->unhistory);
}

void
destroy_history(struct session *ses)
{
	free_history(&ses->history);
	free_history(&ses->unhistory);
}

void
clean_unhistory(struct session *ses)
{
	if (get_opt_int("document.history.keep_unhistory")) return;
	free_history(&ses->unhistory);
}


static void
ses_leave_location(struct session *ses)
{
	free_files(ses);

	if (ses->search_word) {
		mem_free(ses->search_word);
		ses->search_word = NULL;
	}
}

void
ses_back(struct session *ses)
{
	struct location *loc;

	ses_leave_location(ses);

	/* This is the current location. */
	if (!have_location(ses)) return;
	loc = cur_loc(ses);
    	del_from_list(loc);

	if (loc->unhist_jump) {
		/* When going back by multiple steps, to order us properly. */
		add_at_pos(loc->unhist_jump->prev, loc);
		loc->unhist_jump = NULL;
	} else {
		add_to_list(ses->unhistory, loc);
	}

	if (!have_location(ses)) return;

	/* This was the previous location (where we came back now). */
	loc = ses->history.next;

	if (!strcmp(loc->vs.url, ses->loading_url)) return;

	/* Remake that location. */
	destroy_location(loc);
	ses_forward(ses);
}

void
ses_unback(struct session *ses)
{
	ses_leave_location(ses);

	if (!list_empty(ses->unhistory)) {
		struct location *loc = ses->unhistory.next;

		del_from_list(loc);
		/* Save it as the current location! */
		add_to_list(ses->history, loc);
	}
}


void
go_back(struct session *ses)
{
	unsigned char *url;
	struct f_data_c *fd = current_frame(ses);

	ses->reloadlevel = NC_CACHE;
	if (ses->wtd) {
		if (1 || ses->wtd != WTD_BACK) {
			abort_loading(ses, 0);
			print_screen_status(ses);
			reload(ses, NC_CACHE);
		}
		return;
	}

	if (!have_location(ses) || ses->history.next == ses->history.prev)
		/* There's no history, maximally only current location. */
		return;

	abort_loading(ses, 0);

	url = stracpy(((struct location *)ses->history.next)->next->vs.url);
	if (!url) return;

	if (ses->ref_url) {
		mem_free(ses->ref_url);
		ses->ref_url = NULL;
	}

	if (fd && fd->f_data && fd->f_data->url) {
		int l = 0;

		ses->ref_url = init_str();
		if (ses->ref_url)
			add_to_str(&ses->ref_url, &l, fd->f_data->url);
	}

	ses_goto(ses, url, NULL, PRI_MAIN, NC_ALWAYS_CACHE, WTD_BACK, NULL,
		 end_load, 0);
}

void
go_unback(struct session *ses)
{
	unsigned char *url;
	struct f_data_c *fd = current_frame(ses);

	ses->reloadlevel = NC_CACHE;
	/* XXX: why wtd checking is not here? --pasky */
	if (list_empty(ses->unhistory)) return;

	abort_loading(ses, 0);

	url = stracpy(((struct location *)ses->unhistory.next)->vs.url);
	if (!url) return;

	if (ses->ref_url) {
		mem_free(ses->ref_url);
		ses->ref_url = NULL;
	}

	if (fd && fd->f_data && fd->f_data->url) {
		int l = 0;

		ses->ref_url = init_str();
		if (ses->ref_url)
			add_to_str(&ses->ref_url, &l, fd->f_data->url);
	}

	ses_goto(ses, url, NULL, PRI_MAIN, NC_ALWAYS_CACHE, WTD_UNBACK, NULL,
		 end_load, 1);
}
