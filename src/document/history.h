/* $Id: history.h,v 1.3 2002/12/07 20:05:54 pasky Exp $ */

#ifndef EL__DOCUMENT_HISTORY_H
#define EL__DOCUMENT_HISTORY_H

#include "util/lists.h"
#include "document/location.h"
#include "document/session.h"

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
