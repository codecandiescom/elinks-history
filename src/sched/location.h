/* $Id: location.h,v 1.12 2004/03/22 03:59:08 jonas Exp $ */

#ifndef EL__SCHED_LOCATION_H
#define EL__SCHED_LOCATION_H

#include "sched/connection.h"
#include "util/lists.h"
#include "viewer/text/vs.h"

struct location {
	LIST_HEAD(struct location);

	struct list_head frames;
	struct download download;
	struct view_state vs;
};

#define get_location_url(loc) struri((loc)->vs.uri)

void copy_location(struct location *, struct location *);

/* You probably want to call del_from_history() first! */
void destroy_location(struct location *);

#endif
