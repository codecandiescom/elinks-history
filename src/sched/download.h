/* $Id: download.h,v 1.11 2003/07/04 00:54:10 jonas Exp $ */

#ifndef EL__SCHED_DOWNLOAD_H
#define EL__SCHED_DOWNLOAD_H

#include <sys/types.h>

#include "document/cache.h"
#include "terminal/terminal.h"
#include "lowlevel/ttime.h"
#include "terminal/window.h"
#include "sched/connection.h"
#include "sched/session.h"
#include "util/lists.h"

struct file_download {
	LIST_HEAD(struct file_download);

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

int are_there_downloads(void);

void start_download(struct session *, unsigned char *);
void resume_download(struct session *, unsigned char *);
void display_download(struct terminal *, struct file_download *, struct session *);
void create_download_file(struct terminal *, unsigned char *, unsigned char **,
			  int, int,
			  void (*)(struct terminal *, int, void *, int),
			  void *);

void abort_all_downloads(void);
void destroy_downloads(struct session *);

int ses_chktype(struct session *, struct status **, struct cache_entry *);

#endif
