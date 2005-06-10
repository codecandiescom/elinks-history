/* $Id: action.h,v 1.9 2005/06/10 04:47:02 miciah Exp $ */

#ifndef EL__SCHED_ACTION_H
#define EL__SCHED_ACTION_H

#include "config/kbdbind.h"
#include "viewer/text/view.h"

struct session;

enum frame_event_status do_action(struct session *ses,
                                  enum main_action action_id, int verbose);

#endif
