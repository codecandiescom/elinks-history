/* $Id: action.h,v 1.4 2004/01/24 23:50:18 pasky Exp $ */

#ifndef EL__SCHED_ACTION_H
#define EL__SCHED_ACTION_H

#include "config/kbdbind.h"
#include "sched/session.h"

enum main_action do_action(struct session *ses, enum main_action action, int verbose);

#endif
