/* Common widget functions. */
/* $Id: widget.c,v 1.35 2004/11/19 17:19:05 zas Exp $ */

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
