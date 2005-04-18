/* $Id: progress.h,v 1.1 2005/04/18 17:00:25 zas Exp $ */

#ifndef EL__DIALOGS_PROGRESS_H
#define EL__DIALOGS_PROGRESS_H

struct terminal;

unsigned char *get_progress_msg(struct progress *progress, struct terminal *term, int wide, int full, unsigned char *separator);

#endif
