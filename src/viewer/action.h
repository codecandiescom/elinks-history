/* $Id: action.h,v 1.6 2004/06/16 09:43:40 zas Exp $ */

#ifndef EL__SCHED_ACTION_H
#define EL__SCHED_ACTION_H

#include "config/kbdbind.h"
#include "sched/session.h"

enum main_action do_action(struct session *ses, enum main_action action, int verbose);

#endif
