/* Common widget functions. */
/* $Id: widget.c,v 1.34 2004/11/18 00:11:42 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "bfu/dialog.h"
#include "bfu/inphist.h"
#include "bfu/widget.h"
#include "intl/gettext/libintl.h"
#include "terminal/terminal.h"
#include "util/error.h"


static void
display_widget(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	if (widget_data->widget->ops->display)
		widget_data->widget->ops->display(dlg_data, widget_data);
}

void
display_widget_focused(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	dlg_data->focus_selected_widget = 1;
	display_widget(dlg_data, widget_data);
}

void
display_widget_unfocused(struct dialog_data *dlg_data, struct widget_data *widget_data)
{
	dlg_data->focus_selected_widget = 0;
	display_widget(dlg_data, widget_data);
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
	widget_data->info.field.vpos = int_max(0, widget_data->info.field.cpos - widget_data->box.width);
}
