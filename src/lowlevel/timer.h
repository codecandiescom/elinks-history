/* $Id: timer.h,v 1.1 2002/08/11 13:49:45 pasky Exp $ */

#ifndef EL__LOWLEVEL_TIMER_H
#define EL__LOWLEVEL_TIMER_H

extern int timer_duration;

void reset_timer();
void init_timer();
void done_timer();

#endif
