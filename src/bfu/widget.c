/* Common widget functions. */
/* $Id: widget.c,v 1.12 2003/08/01 11:13:44 zas Exp $ */

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
display_dlg_item(struct dialog_data *dialog, struct widget_data *widget,
		 int selected)
{
	if (widget->item->ops->display)
		widget->item->ops->display(widget, dialog, selected);
}

void
dlg_set_history(struct widget_data *di)
{
	unsigned char *s = "";
	int len;

	if ((void *) di->cur_hist != &di->history)
		s = di->cur_hist->d;
	len = strlen(s);
	int_upper_bound(&len, di->item->dlen - 1);
	memcpy(di->cdata, s, len);
	di->cdata[len] = 0;
	di->cpos = len;
	di->vpos = di->cpos - di->l;
	if (di->vpos < 0) di->vpos = 0;
}
