/* $Id: download.h,v 1.3 2002/06/17 08:00:15 pasky Exp $ */

#ifndef EL__DOCUMENT_DOWNLOAD_H
#define EL__DOCUMENT_DOWNLOAD_H

#include "document/cache.h"
#include "document/session.h"
#include "lowlevel/sched.h"
#include "lowlevel/terminal.h"
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
	unsigned char *prog;
	int prog_flags;
	time_t remotetime;
	struct session *ses;
	struct window *win;
	struct window *ask;
};

/* Stack of all running downloads */
extern struct list_head downloads;


unsigned char *subst_file(unsigned char *, unsigned char *);

int are_there_downloads();

void start_download(struct session *, unsigned char *);
void display_download(struct terminal *, struct download *, struct session *);
int create_download_file(struct terminal *, unsigned char *, int);

void abort_all_downloads();
void destroy_downloads(struct session *);

int ses_chktype(struct session *ses, struct status **stat, struct cache_entry *ce);

#endif
