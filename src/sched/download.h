/* $Id: download.h,v 1.24 2003/11/28 03:49:08 jonas Exp $ */

#ifndef EL__SCHED_DOWNLOAD_H
#define EL__SCHED_DOWNLOAD_H

#include <sys/types.h>

#include "cache/cache.h"
#include "lowlevel/ttime.h"
#include "sched/connection.h"
#include "sched/session.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/lists.h"

struct file_download {
	LIST_HEAD(struct file_download);

	unsigned char *url;
	unsigned char *file;
	unsigned char *prog;
	struct session *ses;
	struct window *win;
	ttime remotetime;
	int last_pos;
	int handle;
	int redirect_cnt;
	int notify;
	int prog_flags;
	struct download download;

	/* Should the file be deleted when destroying the structure */
	unsigned int delete:1;

	/* Should any download dialog be updated */
	unsigned int dirty:1;
	struct listbox_item *box_item;
	int refcount;
};

/* Stack of all running downloads */
extern struct list_head downloads;


unsigned char *subst_file(unsigned char *, unsigned char *);

int are_there_downloads(void);

void start_download(void *, unsigned char *);
void resume_download(void *, unsigned char *);
void create_download_file(struct terminal *, unsigned char *, unsigned char **,
			  int, int,
			  void (*)(struct terminal *, int, void *, int),
			  void *);

void abort_all_downloads(void);
void destroy_downloads(struct session *);

int ses_chktype(struct session *, struct download *, struct cache_entry *, int);

void abort_download(struct file_download *down, int stop);

#endif
