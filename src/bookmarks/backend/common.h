/* $Id: common.h,v 1.2 2002/12/11 14:39:09 pasky Exp $ */

#ifndef EL__BOOKMARKS_BACKEND_COMMON_H
#define EL__BOOKMARKS_BACKEND_COMMON_H

#include <stdio.h>
#include "util/lists.h"
#include "util/secsave.h"

struct bookmarks_backend {
	unsigned char *(*filename)(int);
	void (*read)(FILE *);
	void (*write)(struct secure_save_info *, struct list_head *);
};

void bookmarks_read();
void bookmarks_write(struct list_head *);

#endif
