/* $Id: history.h,v 1.6 2003/10/23 22:05:17 pasky Exp $ */

#ifndef EL__SCHED_HISTORY_H
#define EL__SCHED_HISTORY_H

#include "sched/location.h"

struct session;

struct ses_history {
	/* The _last_ visited location is always stored _first_ in the list.
	 * Thus, after visiting A B C D E and then going back to C, in history
	 * will be (in this order, from list.next through ->nexts) B A and in
	 * unhistory will be D E. */
	struct list_head history; /* -> struct location */
	struct list_head unhistory; /* -> struct location */

	/* The current location. */
	/* TODO: This is not actually used now ;-). */
	struct location *current;
};


void create_history(struct ses_history *history);
void destroy_history(struct ses_history *history);
void clean_unhistory(struct ses_history *history);

static inline void
add_to_history(struct ses_history *history, struct location *loc) {
	add_to_list(history->history, loc);
}

static inline void
add_to_unhistory(struct ses_history *history, struct location *loc) {
	add_to_list(history->unhistory, loc);
}


void go_back(struct session *);
void go_unback(struct session *);

void ses_back(struct session *);
void ses_unback(struct session *);

#endif
