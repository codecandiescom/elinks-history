/* $Id: action.h,v 1.5 2004/06/09 22:12:28 jonas Exp $ */

#ifndef EL__SCHED_ACTION_H
#define EL__SCHED_ACTION_H

#include "config/kbdbind.h"
#include "sched/session.h"

enum frame_event_status {
	/* The event was not handled */
	FRAME_EVENT_IGNORED,
	/* The event was handled, and the screen should be redrawn */
	FRAME_EVENT_REFRESH,
	/* The event was handled, and the screen should _not_ be redrawn */
	FRAME_EVENT_OK,
};

enum main_action do_action(struct session *ses, enum main_action action, int verbose);

#endif
