/* Common widget functions. */
/* $Id: widget.c,v 1.5 2002/07/09 23:01:07 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "links.h"

#include "bfu/dialog.h"
#include "bfu/listbox.h"
#include "intl/language.h"
#include "lowlevel/kbd.h"
#include "lowlevel/terminal.h"
#include "util/error.h"


void
display_dlg_item(struct dialog_data *dialog, struct widget_data *widget,
		 int selected)
{
	if (widget->item->ops->display)
		widget->item->ops->display(widget, dialog, selected);
}

/* dlg_select_item() */
void dlg_select_item(struct dialog_data *dlg, struct widget_data *di)
{
	if (di->item->type == D_CHECKBOX) {
		if (!di->item->gid) {
			di->checked = *((int *) di->cdata)
				    = !*((int *) di->cdata);
		} else {
			int i;

			for (i = 0; i < dlg->n; i++) {
				if (dlg->items[i].item->type == D_CHECKBOX
				    && dlg->items[i].item->gid == di->item->gid) {
					*((int *) dlg->items[i].cdata) = di->item->gnum;
					dlg->items[i].checked = 0;
					display_dlg_item(dlg, &dlg->items[i], 0);
				}
			}
			di->checked = 1;
		}
		display_dlg_item(dlg, di, 1);

	} else {
		if (di->item->type == D_BUTTON)
			di->item->fn(dlg, di);
	}
}

/* dlg_set_history() */
void dlg_set_history(struct widget_data *di)
{
	unsigned char *s = "";
	int len;

	if ((void *) di->cur_hist != &di->history)
		s = di->cur_hist->d;
	len = strlen(s);
	if (len > di->item->dlen)
		len = di->item->dlen - 1;
	memcpy(di->cdata, s, len);
	di->cdata[len] = 0;
	di->cpos = len;
}
