/* Common widget functions. */
/* $Id: widget.c,v 1.27 2004/04/16 10:02:06 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "bfu/inphist.h"
#include "intl/gettext/libintl.h"
#include "terminal/kbd.h"
#include "terminal/terminal.h"
#include "util/error.h"
#include "util/math.h"


void
display_dlg_item(struct dialog_data *dlg_data, struct widget_data *widget_data,
		 int selected)
{
	if (widget_data->widget->ops->display)
		widget_data->widget->ops->display(widget_data, dlg_data, selected);
}

/* XXX: Should we move it to inphist.c since it only concerns fields with history ? --Zas */
void
dlg_set_history(struct widget_data *widget_data)
{
	assert(widget_has_history(widget_data));
	assert(widget_data->widget->datalen > 0);

	if ((void *) widget_data->info.field.cur_hist != &widget_data->info.field.history) {
		unsigned char *s = widget_data->info.field.cur_hist->data;

		widget_data->info.field.cpos = int_min(strlen(s), widget_data->widget->datalen - 1);
		if (widget_data->info.field.cpos)
			memcpy(widget_data->cdata, s, widget_data->info.field.cpos);
	} else {
		widget_data->info.field.cpos = 0;
	}

	widget_data->cdata[widget_data->info.field.cpos] = 0;
	widget_data->info.field.vpos = int_max(0, widget_data->info.field.cpos - widget_data->w);
}
