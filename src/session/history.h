/* $Id: history.h,v 1.1 2003/01/05 16:48:16 pasky Exp $ */

#ifndef EL__SCHED_HISTORY_H
#define EL__SCHED_HISTORY_H

#include "sched/location.h"
#include "sched/session.h"
#include "util/lists.h"

void go_back(struct session *);
void go_unback(struct session *);

void ses_back(struct session *);
void ses_unback(struct session *);

void create_history(struct session *);
void destroy_history(struct session *);
void clean_unhistory(struct session *);

/* Return if we have anything being loaded in this session already. If you
 * don't understand, please read top of history.c about ses->history. */
static inline int
have_location(struct session *ses) {
	return !list_empty(ses->history);
}

static inline void
add_to_history(struct session *ses, struct location *loc) {
	add_to_list(ses->history, loc);
}

#endif
