/* $Id: status.h,v 1.2 2003/12/02 19:16:34 jonas Exp $ */

#ifndef EL__DIALOGS_STATUS_H
#define EL__DIALOGS_STATUS_H

#include "document/options.h"
#include "terminal/terminal.h"
#include "sched/connection.h"
#include "sched/session.h"

void print_screen_status(struct session *);

void update_status(void);
void init_bars_status(struct session *, int *, struct document_options *);

unsigned char *
get_stat_msg(struct download *stat, struct terminal *term,
	     int wide, int full, unsigned char *separator);


#endif
