/* $Id: action.h,v 1.3 2004/01/14 17:10:01 jonas Exp $ */

#ifndef EL__SCHED_ACTION_H
#define EL__SCHED_ACTION_H

#include "config/kbdbind.h"
#include "sched/session.h"

enum action do_action(struct session *ses, enum action action, int verbose);

#endif
