/* Manipulation with file containing URL history */
/* $Id: urlhist.c,v 1.1 2002/04/28 12:00:27 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include <links.h>

#include <bfu/bfu.h>
#include <config/options.h>
#include <lowlevel/home.h>
#include <util/secsave.h>


/* FIXME: This should be in some .h file! */
extern struct input_history goto_url_history;

/* Load history file */
int load_url_history()
{
	FILE *fp;
	unsigned char *history_file;
	unsigned char url[MAX_STR_LEN];

	if (anonymous) return 0;
	/* Must have been called after init_home */
	/* if (!links_home) return 0; */ /* strconcat() checks it --Zas */

	history_file = straconcat(links_home, "links.his", NULL);
	if (!history_file) return 0;

	fp = fopen(history_file, "r");
	if (!fp) {
		mem_free(history_file);
		return 0;
	}

	while (fgets(url, MAX_STR_LEN, fp)) {
		url[strlen(url) - 1] = 0;
		add_to_input_history(&goto_url_history, url, 0);
	}

	fclose(fp);
	mem_free(history_file);
	return 0;
}

/* Write history list to file. It returns a value different from 0 in case of
 * failure, 0 on success. */
int
save_url_history()
{
	struct input_history_item* historyitem;
	struct secure_save_info *ssi;
	unsigned char *history_file;
	int i = 0;

	if (anonymous) return 0;

	history_file = straconcat(links_home, "links.his", NULL);
	if (!history_file) return -1;

	ssi = secure_open(history_file, 0177);
	mem_free(history_file);
	if (!ssi) return -1;
	
	foreachback(historyitem, goto_url_history.items) {
		if (i++ > MAX_HISTORY_ITEMS) break;
		secure_fputs(ssi, historyitem->d);
		secure_fputc(ssi, '\n');
		if (ssi->err) break;
	}

	return secure_close(ssi);
}
