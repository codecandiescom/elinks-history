/* Manipulation with file containing URL history */
/* $Id: urlhist.c,v 1.21 2003/05/13 22:37:30 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "bfu/inphist.h"
#include "config/options.h"
#include "config/urlhist.h"
#include "lowlevel/home.h"
#include "util/file.h"
#include "util/memory.h"
#include "util/secsave.h"
#include "util/string.h"


/* FIXME: This should be in some .h file! */
extern struct input_history goto_url_history;

int history_dirty = 0;
int history_nosave = 0;

/* Load history file */
int
load_url_history(void)
{
	FILE *fp;
	unsigned char *history_file = "gotohist";
	unsigned char url[MAX_STR_LEN];

	if (get_opt_int_tree(&cmdline_options, "anonymous")) return 0;
	if (elinks_home) {
		history_file = straconcat(elinks_home, "gotohist", NULL);
		if (!history_file) return 0;
	}

	fp = fopen(history_file, "r");
	if (elinks_home) mem_free(history_file);
	if (!fp) return 0;

	history_nosave = 1;
	while (safe_fgets(url, MAX_STR_LEN, fp)) {
		/* Drop '\n'. */
		if (*url) url[strlen(url) - 1] = 0;
		add_to_input_history(&goto_url_history, url, 0);
	}
	history_nosave = 0;

	fclose(fp);
	return 0;
}

/* Write history list to file. It returns a value different from 0 in case of
 * failure, 0 on success. */
int
save_url_history(void)
{
	struct input_history_item *historyitem;
	struct secure_save_info *ssi;
	unsigned char *history_file;
	int i = 0;
	int ret;

	if (!history_dirty
	    || !elinks_home
	    || get_opt_int_tree(&cmdline_options, "anonymous"))
		return 0;

	history_file = straconcat(elinks_home, "gotohist", NULL);
	if (!history_file) return -1;

	ssi = secure_open(history_file, 0177);
	mem_free(history_file);
	if (!ssi) return -1;

	foreachback (historyitem, goto_url_history.items) {
		if (i++ > MAX_HISTORY_ITEMS) break;
		secure_fputs(ssi, historyitem->d);
		secure_fputc(ssi, '\n');
		if (ssi->err) break;
	}

	ret = secure_close(ssi);
	if (!ret) history_dirty = 0;

	return ret;
}
