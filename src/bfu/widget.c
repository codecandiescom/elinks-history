/* Common widget functions. */
/* $Id: widget.c,v 1.15 2003/10/26 12:52:32 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "bfu/listbox.h"
#include "intl/gettext/libintl.h"
#include "terminal/kbd.h"
#include "terminal/terminal.h"
#include "util/error.h"


void
display_dlg_item(struct dialog_data *dlg_data, struct widget_data *di,
		 int selected)
{
	if (di->widget->ops->display)
		di->widget->ops->display(di, dlg_data, selected);
}

void
dlg_set_history(struct widget_data *di)
{
	assert(di->widget->dlen > 0);

	if ((void *) di->cur_hist != &di->history) {
		unsigned char *s = di->cur_hist->d;

		di->cpos = int_min(strlen(s), di->widget->dlen - 1);
		if (di->cpos) memcpy(di->cdata, s, di->cpos);
	} else {
		di->cpos = 0;
	}

	di->cdata[di->cpos] = 0;
	di->vpos = int_max(0, di->cpos - di->l);
}
