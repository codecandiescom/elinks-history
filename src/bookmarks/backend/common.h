/* $Id: common.h,v 1.1 2002/12/01 19:02:09 pasky Exp $ */

#ifndef EL__BOOKMARKS_BACKEND_COMMON_H
#define EL__BOOKMARKS_BACKEND_COMMON_H

#include <stdio.h>
#include "util/lists.h"
#include "util/secsave.h"

struct bookmarks_backend {
	void (*read)(FILE *);
	void (*write)(struct secure_save_info *, struct list_head *);
};

void bookmarks_read();
void bookmarks_write(struct list_head *);

#endif
