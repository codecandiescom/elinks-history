/* $Id: action.h,v 1.11 2005/06/10 21:07:53 jonas Exp $ */

#ifndef EL__SCHED_ACTION_H
#define EL__SCHED_ACTION_H

#include "config/kbdbind.h"
#include "viewer/text/view.h"

struct session;

enum frame_event_status do_action(struct session *ses,
                                  enum main_action action_id, int verbose);

#endif
