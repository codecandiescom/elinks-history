/* $Id: location.h,v 1.9 2003/10/23 22:15:17 pasky Exp $ */

#ifndef EL__SCHED_LOCATION_H
#define EL__SCHED_LOCATION_H

#include "sched/connection.h"
#include "util/lists.h"
#include "viewer/text/vs.h"

struct location {
	LIST_HEAD(struct location);

	struct list_head frames;
	struct download download;
	struct view_state vs; /* has to be last */
};


void copy_location(struct location *, struct location *);

/* You probably want to call del_from_history() first! */
void destroy_location(struct location *);

#endif
