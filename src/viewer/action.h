/* $Id: action.h,v 1.8 2005/04/21 02:35:52 jonas Exp $ */

#ifndef EL__SCHED_ACTION_H
#define EL__SCHED_ACTION_H

#include "config/kbdbind.h"
#include "viewer/text/view.h"

struct session;

enum frame_event_status do_action(struct session *ses, enum main_action action,
				  int verbose);

#endif
