/* $Id: task.h,v 1.3 2003/12/06 17:04:32 pasky Exp $ */

#ifndef EL__SCHED_TASK_H
#define EL__SCHED_TASK_H

#include "sched/session.h"

struct dowload;
struct location;
struct terminal;
struct view_state;

/* This is for map_selected(), it is used to pass around information about
 * in-imagemap links. */
struct link_def {
	unsigned char *link;
	unsigned char *target;
};

void abort_preloading(struct session *, int);

void ses_goto(struct session *, unsigned char *, unsigned char *, struct location *,
	      int, enum cache_mode, enum task_type, unsigned char *,
	      void (*)(struct download *, struct session *), int);
struct view_state *ses_forward(struct session *, int);

void end_load(struct download *, struct session *);

void goto_url_frame_reload(struct session *, unsigned char *, unsigned char *);
void goto_url_frame(struct session *, unsigned char *, unsigned char *);
void goto_url(struct session *, unsigned char *);
void goto_url_with_hook(struct session *, unsigned char *);
void goto_imgmap(struct session *, unsigned char *, unsigned char *, unsigned char *);
void map_selected(struct terminal *, struct link_def *, struct session *);

#endif
