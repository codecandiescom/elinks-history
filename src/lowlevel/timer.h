/* $Id: timer.h,v 1.6 2005/04/07 11:16:57 jonas Exp $ */

#ifndef EL__LOWLEVEL_TIMER_H
#define EL__LOWLEVEL_TIMER_H

struct module;

int get_timer_duration(void);
void reset_timer(void);

extern struct module timer_module;

#endif
