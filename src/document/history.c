/* Visited URL history managment - NOT goto_url_dialog history! */
/* $Id: history.c,v 1.12 2002/12/01 19:42:34 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "links.h"

#include "config/options.h"
#include "document/session.h"
#include "document/history.h"
#include "document/location.h"
#include "document/view.h"
#include "lowlevel/sched.h"
#include "util/memory.h"
#include "util/string.h"

/* The history itself is stored in struct session as field history,
 * surprisingly. It's a list containing all locations visited in the current
 * session, including the one being visited just now! So the location on the
 * top of the list is the current location.
 *
 * The unhistory is reverse of history, it contains locations which you
 * visited, but then got bored and went back. The fields pushed away from
 * history are moved to unhistory. There's nothing special on the first item of
 * unhistory. */


void
free_history(struct list_head *history)
{
	struct location *loc = history->next;

	/* We can't do foreach() loop here, we access freed memory somehow. */
	while ((struct list_head *) loc != history) {
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


void
ses_back(struct session *ses)
{
	struct location *loc;

	free_files(ses);

	/* This is the current location. */
	loc = cur_loc(ses);
	if (ses->search_word) {
		mem_free(ses->search_word);
		ses->search_word = NULL;
	}
	if (!have_location(ses)) return;
    	del_from_list(loc);
	if (loc->unhist_jump) {
		/* When going back by multiple steps, to order us properly. */
		add_at_pos(loc->unhist_jump->prev, loc);
		loc->unhist_jump = NULL;
	} else {
		add_to_list(ses->unhistory, loc);
	}

	/* This was the previous location (where we came back now). */
	loc = ses->history.next;
	if (!have_location(ses)) return;
	if (!strcmp(loc->vs.url, ses->loading_url)) return;

	/* Remake that location. */
	destroy_location(loc);
	ses_forward(ses);
}

void
ses_unback(struct session *ses)
{
	struct location *loc;

	free_files(ses);

	loc = ses->unhistory.next;
	if (ses->search_word) {
		mem_free(ses->search_word);
		ses->search_word = NULL;
	}
	if (list_empty(ses->unhistory)) return;
	del_from_list(loc);
	/* Save it as the current location! */
	add_to_list(ses->history, loc);
}


void
go_back(struct session *ses)
{
	unsigned char *url;
	struct f_data_c *fd = current_frame(ses);
	int l = 0;

	ses->reloadlevel = NC_CACHE;
	if (ses->wtd) {
		if (1 || ses->wtd != WTD_BACK) {
			abort_loading(ses);
			print_screen_status(ses);
			reload(ses, NC_CACHE);
		}
		return;
	}
	if (!have_location(ses) || ses->history.next == ses->history.prev)
		/* There's no history, maximally only current location. */
		return;
	abort_loading(ses);

	url = stracpy(((struct location *)ses->history.next)->next->vs.url);
	if (!url) return;

	if (ses->ref_url) {
		mem_free(ses->ref_url);
		ses->ref_url=NULL;
	}
	if (fd && fd->f_data && fd->f_data->url) {
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
	int l = 0;

	ses->reloadlevel = NC_CACHE;
	/* XXX: why wtd checking is not here? --pasky */
	if (list_empty(ses->unhistory)) return;

	abort_loading(ses);

	url = stracpy(((struct location *)ses->unhistory.next)->vs.url);
	if (!url) return;

	if (ses->ref_url) {
		mem_free(ses->ref_url);
		ses->ref_url=NULL;
	}
	if (fd && fd->f_data && fd->f_data->url) {
		ses->ref_url = init_str();
		if (ses->ref_url)
			add_to_str(&ses->ref_url, &l, fd->f_data->url);
	}

	ses_goto(ses, url, NULL, PRI_MAIN, NC_ALWAYS_CACHE, WTD_UNBACK, NULL,
		 end_load, 1);
}
