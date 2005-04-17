/* $Id: download.h,v 1.7 2005/04/17 16:22:05 zas Exp $ */

#ifndef EL__DIALOGS_DOWNLOAD_H
#define EL__DIALOGS_DOWNLOAD_H

struct file_download;
struct session;
struct terminal;

void init_download_display(struct file_download *file_download);
void done_download_display(struct file_download *file_download);

void display_download(struct terminal *, struct file_download *, struct session *);
void download_manager(struct session *ses);
extern struct hierbox_browser download_browser;

#endif
