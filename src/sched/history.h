/* $Id: history.h,v 1.3 2003/06/11 22:42:46 pasky Exp $ */

#ifndef EL__SCHED_HISTORY_H
#define EL__SCHED_HISTORY_H

#include "sched/location.h"
#include "sched/session.h"


void create_history(struct session *);
void destroy_history(struct session *);
void clean_unhistory(struct session *);

static inline void
add_to_history(struct session *ses, struct location *loc) {
	add_to_list(ses->history, loc);
}


void go_back(struct session *);
void go_unback(struct session *);

void ses_back(struct session *);
void ses_unback(struct session *);

#endif
