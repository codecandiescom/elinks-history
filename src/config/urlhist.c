/* Manipulation with file containing URL history */
/* $Id: urlhist.c,v 1.27 2003/10/27 15:33:23 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "elinks.h"

#include "bfu/inphist.h"
#include "config/urlhist.h"
#include "util/lists.h"


struct input_history goto_url_history = {
	/* n: */	0,
	/* items: */	{ D_LIST_HEAD(goto_url_history.items) },
	/* dirty: */	0,
	/* nosave: */	0,
};

void
load_url_history(void)
{
	load_input_history(&goto_url_history, "gotohist");
}

void
save_url_history(void)
{
	save_input_history(&goto_url_history, "gotohist");
}
