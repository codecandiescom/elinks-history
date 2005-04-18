/* $Id: progress.h,v 1.2 2005/04/18 17:13:03 zas Exp $ */

#ifndef EL__DIALOGS_PROGRESS_H
#define EL__DIALOGS_PROGRESS_H

struct progress;
struct terminal;

unsigned char *get_progress_msg(struct progress *progress, struct terminal *term, int wide, int full, unsigned char *separator);

#endif
