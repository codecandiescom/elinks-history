/* $Id: action.h,v 1.2 2004/01/07 14:44:15 jonas Exp $ */

#ifndef EL__SCHED_ACTION_H
#define EL__SCHED_ACTION_H

#include "config/kbdbind.h"
#include "sched/session.h"

enum keyact do_action(struct session *ses, enum keyact action, int verbose);

#endif
