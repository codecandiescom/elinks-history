/* $Id: task.h,v 1.11 2004/06/08 13:49:10 jonas Exp $ */

#ifndef EL__SCHED_TASK_H
#define EL__SCHED_TASK_H

#include "cache/cache.h"
#include "sched/session.h"

struct download;
struct location;
struct terminal;
struct view_state;
struct uri;

/* This is for map_selected(), it is used to pass around information about
 * in-imagemap links. */
struct link_def {
	unsigned char *link;
	unsigned char *target;
};

void abort_preloading(struct session *, int);

void ses_goto(struct session *, struct uri *, unsigned char *,
	      struct location *, enum cache_mode, enum task_type, int);
struct view_state *ses_forward(struct session *, int);

void end_load(struct download *, struct session *);

void goto_url_frame(struct session *, struct uri *, unsigned char *, enum cache_mode);
void goto_url(struct session *, unsigned char *);
void goto_url_with_hook(struct session *, unsigned char *);
int goto_url_home(struct session *ses);
void goto_imgmap(struct session *, struct uri *, unsigned char *);
void map_selected(struct terminal *, struct link_def *, struct session *);

#endif
