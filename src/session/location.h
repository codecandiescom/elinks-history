/* $Id: location.h,v 1.5 2003/06/15 14:05:11 jonas Exp $ */

#ifndef EL__SCHED_LOCATION_H
#define EL__SCHED_LOCATION_H

#include "sched/sched.h"
#include "util/lists.h"
#include "viewer/text/vs.h"

struct location {
	LIST_HEAD(struct location);

	struct list_head frames;
	struct status stat;
	struct view_state vs; /* has to be last */
};


void copy_location(struct location *, struct location *);

void destroy_location(struct location *);

void set_session_location(struct session *ses, struct location *loc, int direction);

#endif
