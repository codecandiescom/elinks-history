/* $Id: download.h,v 1.1 2003/11/26 01:29:20 jonas Exp $ */

#ifndef EL__DIALOGS_DOWNLOAD_H
#define EL__DIALOGS_DOWNLOAD_H

#include "sched/download.h"
#include "sched/session.h"
#include "terminal/terminal.h"

void display_download(struct terminal *, struct file_download *, struct session *);

#endif
