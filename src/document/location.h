/* $Id: location.h,v 1.7 2003/01/01 18:19:52 pasky Exp $ */

#ifndef EL__DOCUMENT_LOCATION_H
#define EL__DOCUMENT_LOCATION_H

#include "lowlevel/sched.h"
#include "util/lists.h"
#include "viewer/text/vs.h"

struct location {
	struct location *next;
	struct location *prev;

	/* In order to move stuff around properly when going back by multiple
	 * steps, we need this temporary pointer to pass the proper unhistory
	 * slot by. */
	struct location *unhist_jump;

	struct list_head frames;
	struct status stat;
	struct view_state vs; /* has to be last */
};

#define cur_loc(x) ((struct location *) ((x)->history.next))


void copy_location(struct location *, struct location *);

void destroy_location(struct location *);

#endif
