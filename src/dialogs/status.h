/* $Id: status.h,v 1.3 2003/12/02 20:53:17 jonas Exp $ */

#ifndef EL__DIALOGS_STATUS_H
#define EL__DIALOGS_STATUS_H

#include "terminal/terminal.h"
#include "sched/connection.h"
#include "sched/session.h"

void print_screen_status(struct session *);

void update_status(void);

unsigned char *
get_stat_msg(struct download *stat, struct terminal *term,
	     int wide, int full, unsigned char *separator);


#endif
