/* $Id: timer.h,v 1.4 2005/03/04 13:19:37 zas Exp $ */

#ifndef EL__LOWLEVEL_TIMER_H
#define EL__LOWLEVEL_TIMER_H

extern int timer_duration;

void reset_timer(void);
void init_timer(void);
void done_timer(void);

#endif
