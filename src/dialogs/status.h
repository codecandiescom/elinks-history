/* $Id: status.h,v 1.4 2004/01/04 18:56:35 jonas Exp $ */

#ifndef EL__DIALOGS_STATUS_H
#define EL__DIALOGS_STATUS_H

#include "terminal/terminal.h"
#include "sched/connection.h"
#include "sched/session.h"

#define download_is_progressing(down) \
	((down) && (down)->state == S_TRANS && ((down)->prg->elapsed / 100))

void print_screen_status(struct session *);

void update_status(void);

unsigned char *
get_stat_msg(struct download *stat, struct terminal *term,
	     int wide, int full, unsigned char *separator);


#endif
