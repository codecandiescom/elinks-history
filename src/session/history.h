/* $Id: history.h,v 1.15 2003/10/24 20:39:38 pasky Exp $ */

#ifndef EL__SCHED_HISTORY_H
#define EL__SCHED_HISTORY_H

#include "sched/location.h"

struct session;

struct ses_history {
	/* The first list item is the first visited location. The last list
	 * item is the last location in the unhistory. The @current location is
	 * included in this list. */
	struct list_head history; /* -> struct location */

	/* The current location. This is moveable pivot pointing somewhere at
	 * the middle of @history. */
	struct location *current;
};


void create_history(struct ses_history *history);
void destroy_history(struct ses_history *history);
void clean_unhistory(struct ses_history *history);

static inline void
add_to_history(struct ses_history *history, struct location *loc) {
	if (!history->current)
		add_to_list(history->history, loc);
	else
		add_at_pos(history->current, loc);
	history->current = loc;
}

static inline void
del_from_history(struct ses_history *history, struct location *loc) {
	del_from_list(loc);
	if (history->current == loc)
		history->current = loc->prev;
	if (history->current == (struct location *) &history->history)
		history->current = loc->next;
	if (history->current == (struct location *) &history->history)
		history->current = NULL;
}


void go_back(struct session *, struct location *);
void go_unback(struct session *, struct location *);

void ses_history_move(struct session *);

#endif
