/* $Id: action.h,v 1.10 2005/06/10 21:05:07 jonas Exp $ */

#ifndef EL__SCHED_ACTION_H
#define EL__SCHED_ACTION_H

#include "config/kbdbind.h"

struct session;

enum frame_event_status do_action(struct session *ses,
                                  enum main_action action_id, int verbose);

#endif
