/* Internal bookmarks support - file format backends multiplexing */
/* $Id: common.c,v 1.6 2002/12/11 13:18:14 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/listbox.h"
#include "bookmarks/bookmarks.h"
#include "bookmarks/backend/common.h"
#include "lowlevel/home.h"
#include "util/memory.h"
#include "util/secsave.h"
#include "util/string.h"


/* Backends dynamic area: */

#include "bookmarks/backend/default.h"
#include "bookmarks/backend/xbel.h"

/* Note that the numbering is static, that means that you have to provide at
 * least dummy NULL handlers even when no support is compiled in. */

static struct bookmarks_backend *bookmarks_backends[] = {
	&default_bookmarks_backend,
	&xbel_bookmarks_backend,
};

/* :aera cimanyd sdnekcaB */


#ifdef BOOKMARKS

/* Loads the bookmarks from file */
void
bookmarks_read()
{
	int backend = get_opt_int("bookmarks.file_format");
	unsigned char *file_name;
	FILE *f;

	if (!bookmarks_backends[backend]->read) return;

	file_name = straconcat(elinks_home, "bookmarks", NULL);
	if (!file_name) return;

	f = fopen(file_name, "r");
	mem_free(file_name);
	if (!f) return;

	bookmarks_backends[backend]->read(f);

	fclose(f);
	bookmarks_dirty = 0;
#undef INBUF_SIZE
}

void
bookmarks_write(struct list_head *bookmarks)
{
	int backend = get_opt_int("bookmarks.file_format");
	struct secure_save_info *ssi;
	unsigned char *file_name;

	if (!bookmarks_dirty) return;
	if (!bookmarks_backends[backend]->write) return;

	file_name = straconcat(elinks_home, "bookmarks", NULL);
	if (!file_name) return;

	ssi = secure_open(file_name, 0177);
	mem_free(file_name);
	if (!ssi) return;

	bookmarks_backends[backend]->write(ssi, bookmarks);

	if (!secure_close(ssi)) bookmarks_dirty = 0;
}

#endif /* BOOKMARKS */
