/* $Id: download.h,v 1.5 2005/04/17 15:59:54 zas Exp $ */

#ifndef EL__DIALOGS_DOWNLOAD_H
#define EL__DIALOGS_DOWNLOAD_H

#include "bfu/hierbox.h"
#include "sched/download.h"
#include "sched/session.h"
#include "terminal/terminal.h"

void display_download(struct terminal *, struct file_download *, struct session *);
void download_manager(struct session *ses);
extern struct hierbox_browser download_browser;

#endif
