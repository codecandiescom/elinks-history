/* $Id: status.h,v 1.7 2004/10/08 16:25:13 zas Exp $ */

#ifndef EL__DIALOGS_STATUS_H
#define EL__DIALOGS_STATUS_H

#include "terminal/terminal.h"
#include "sched/connection.h"
#include "sched/session.h"

#define download_is_progressing(download) \
	((download) && \
	 (download)->state == S_TRANS && \
	 ((download)->prg->elapsed / 100))

void print_screen_status(struct session *);

void update_status(void);

unsigned char *
get_download_msg(struct download *download, struct terminal *term,
	         int wide, int full, unsigned char *separator);


#endif
