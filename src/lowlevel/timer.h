/* $Id: timer.h,v 1.5 2005/03/04 13:25:32 zas Exp $ */

#ifndef EL__LOWLEVEL_TIMER_H
#define EL__LOWLEVEL_TIMER_H

int get_timer_duration(void);
void reset_timer(void);
void init_timer(void);
void done_timer(void);

#endif
