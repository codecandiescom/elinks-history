/* $Id: download.h,v 1.2 2003/01/22 00:49:19 pasky Exp $ */

#ifndef EL__SCHED_DOWNLOAD_H
#define EL__SCHED_DOWNLOAD_H

#include <sys/types.h>

#include "document/cache.h"
#include "lowlevel/terminal.h"
#include "lowlevel/ttime.h"
#include "sched/session.h"
#include "sched/sched.h"
#include "util/lists.h"

struct download {
	struct download *next;
	struct download *prev;
	unsigned char *url;
	struct status stat;
	unsigned char *file;
	int last_pos;
	int handle;
	int redirect_cnt;
	int notify;
	unsigned char *prog;
	int prog_flags;
	ttime remotetime;
	struct session *ses;
	struct window *win;
	struct window *ask;
};

/* Stack of all running downloads */
extern struct list_head downloads;


unsigned char *subst_file(unsigned char *, unsigned char *);

int are_there_downloads();

void start_download(struct session *, unsigned char *);
void resume_download(struct session *, unsigned char *);
void display_download(struct terminal *, struct download *, struct session *);
void create_download_file(struct terminal *, unsigned char *, unsigned char **, int, int, void (*)(struct terminal *, int, void *), void *);

void abort_all_downloads();
void destroy_downloads(struct session *);

int ses_chktype(struct session *ses, struct status **stat, struct cache_entry *ce);

#endif
