/* $Id: task.h,v 1.1 2003/12/06 02:56:02 jonas Exp $ */

#ifndef EL__SCHED_TASK_H
#define EL__SCHED_TASK_H

#include "terminal/terminal.h"
#include "sched/session.h"

/* This is for map_selected(), it is used to pass around information about
 * in-imagemap links. */
struct link_def {
	unsigned char *link;
	unsigned char *target;
};

void goto_url_frame_reload(struct session *, unsigned char *, unsigned char *);
void goto_url_frame(struct session *, unsigned char *, unsigned char *);
void goto_url(struct session *, unsigned char *);
void goto_url_with_hook(struct session *, unsigned char *);
void goto_imgmap(struct session *, unsigned char *, unsigned char *, unsigned char *);
void map_selected(struct terminal *, struct link_def *, struct session *);

#endif
