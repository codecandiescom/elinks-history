/* $Id: download.h,v 1.37 2004/04/21 22:19:55 jonas Exp $ */

#ifndef EL__SCHED_DOWNLOAD_H
#define EL__SCHED_DOWNLOAD_H

#include "sched/connection.h"
#include "util/lists.h"
#include "util/object.h"
#include "util/ttime.h"

/* Silly BFU stuff */
struct dialog_data;
struct listbox_item;
struct terminal;

struct cache_entry;
struct session;
struct uri;


struct file_download {
	LIST_HEAD(struct file_download);

	struct uri *uri;
	unsigned char *file;
	unsigned char *prog;
	struct session *ses;
	struct terminal *term;
	ttime remotetime;
	int last_pos;
	int handle;
	int redirect_cnt;
	int notify;
	int prog_flags;
	struct download download;

	/* Should the file be deleted when destroying the structure */
	unsigned int delete:1;

	/* Should the download be stopped/interrupted when destroying the structure */
	unsigned int stop:1;

	/* The current dialog for this download. Can be NULL. */
	struct dialog_data *dlg_data;
	struct listbox_item *box_item;
	struct object object;
};

/* Stack of all running downloads */
extern struct list_head downloads;

static inline int
is_in_downloads_list(struct file_download *file_download)
{
	struct file_download *down;

	foreach (down, downloads)
		if (file_download == down) return 1;

	return 0;
}


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

void abort_download(struct file_download *file_download);

#endif
