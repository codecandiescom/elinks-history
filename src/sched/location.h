/* $Id: location.h,v 1.7 2003/07/03 01:03:35 jonas Exp $ */

#ifndef EL__SCHED_LOCATION_H
#define EL__SCHED_LOCATION_H

#include "sched/connection.h"
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

#endif
