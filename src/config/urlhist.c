/* Manipulation with file containing URL history */
/* $Id: urlhist.c,v 1.26 2003/10/05 19:55:18 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "bfu/inphist.h"
#include "config/urlhist.h"
#include "util/lists.h"


struct input_history goto_url_history = { 0, {D_LIST_HEAD(goto_url_history.items)} };

int history_dirty = 0;
int history_nosave = 0;

int
load_url_history(void)
{
	history_nosave = 1;
	load_input_history(&goto_url_history, "gotohist");
	history_nosave = 0;

	return 0;
}

int
save_url_history(void)
{
	if (!history_dirty) return 0;

	if (!save_input_history(&goto_url_history, "gotohist"))
		history_dirty = 0;

	return history_dirty;
}
