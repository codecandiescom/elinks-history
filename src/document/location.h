/* $Id: location.h,v 1.1 2002/03/28 22:26:11 pasky Exp $ */

#ifndef EL__DOCUMENT_LOCATION_H
#define EL__DOCUMENT_LOCATION_H

/* We need to declare this first :/. Damn cross-dependencies. */
struct location;

/* The document/view.h dependency is breaking a lot of things :(. */
#include <document/view.h>
#include <lowlevel/sched.h>

struct location {
	struct location *next;
	struct location *prev;
	struct list_head frames;
	struct status stat;
	struct view_state vs;
};

#define cur_loc(x) ((struct location *) ((x)->history.next))


void destroy_location(struct location *);

#endif
