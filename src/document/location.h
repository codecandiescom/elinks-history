/* $Id: location.h,v 1.3 2002/03/28 22:59:17 pasky Exp $ */

#ifndef EL__DOCUMENT_LOCATION_H
#define EL__DOCUMENT_LOCATION_H

#include <document/vs.h>
#include <lowlevel/sched.h>

struct location {
	struct location *next;
	struct location *prev;
	struct list_head frames;
	struct status stat;
	struct view_state vs;
};

#define cur_loc(x) ((struct location *) ((x)->history.next))


void copy_location(struct location *, struct location *);

void destroy_location(struct location *);

#endif
