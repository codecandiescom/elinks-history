/* $Id: download.h,v 1.2 2003/11/26 04:43:52 jonas Exp $ */

#ifndef EL__DIALOGS_DOWNLOAD_H
#define EL__DIALOGS_DOWNLOAD_H

#include "bfu/hierbox.h"
#include "sched/download.h"
#include "sched/session.h"
#include "terminal/terminal.h"

void display_download(struct terminal *, struct file_download *, struct session *);
void menu_download_manager(struct terminal *term, void *fcp, struct session *ses);
extern struct hierbox_browser download_browser;

#endif
