/* $Id: history.h,v 1.4 2003/06/12 00:47:47 jonas Exp $ */

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

static inline void
add_to_unhistory(struct session *ses, struct location *loc) {
	add_to_list(ses->unhistory, loc);
}


void go_back(struct session *);
void go_unback(struct session *);

void ses_back(struct session *);
void ses_unback(struct session *);

#endif
