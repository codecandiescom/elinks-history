/* Manipulation with file containing URL history */
/* $Id: urlhist.c,v 1.31 2004/02/06 22:41:04 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "bfu/inphist.h"
#include "config/urlhist.h"
#include "util/lists.h"

#define GOTO_HISTORY_FILENAME		"gotohist"


INIT_INPUT_HISTORY(goto_url_history);

void
load_url_history(void)
{
	load_input_history(&goto_url_history, GOTO_HISTORY_FILENAME);
}

void
save_url_history(void)
{
	save_input_history(&goto_url_history, GOTO_HISTORY_FILENAME);
}
