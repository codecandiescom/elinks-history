/* $Id: widget.h,v 1.79 2004/11/19 17:14:56 zas Exp $ */

#ifndef EL__BFU_WIDGET_H
#define EL__BFU_WIDGET_H

#include "bfu/common.h"

/* Dynamic widget #include area */
#include "bfu/button.h"
#include "bfu/checkbox.h"
#include "bfu/group.h"
#include "bfu/inpfield.h"
#include "bfu/inphist.h"
#include "bfu/msgbox.h"
#include "bfu/leds.h"
#include "bfu/listbox.h"
#include "bfu/style.h"
#include "bfu/text.h"
#include "util/lists.h"
#include "util/box.h"

struct dialog_data;
struct widget_data;


struct widget_ops {
	/* XXX: Order matters here. --Zas */
	t_handler_event_status (*display)(struct dialog_data *, struct widget_data *);
	t_handler_event_status (*init)(struct dialog_data *, struct widget_data *);
	t_handler_event_status (*mouse)(struct dialog_data *, struct widget_data *);
	t_handler_event_status (*kbd)(struct dialog_data *, struct widget_data *);
	t_handler_event_status (*select)(struct dialog_data *, struct widget_data *);
};


struct widget {
	struct widget_ops *ops;

	unsigned char *text;
	unsigned char *data;

	void *udata;

	WIDGET_HANDLER_FUNC(fn);

	union {
		struct widget_info_checkbox checkbox;
		struct widget_info_field field;
		struct widget_info_listbox box;
		struct widget_info_button button;
		struct widget_info_text text;
	} info;

	int datalen;

	enum widget_type type;
};

struct widget_data {
	struct widget *widget;
	unsigned char *cdata;

	struct box box;

	union {
		struct widget_data_info_field field;
		struct widget_data_info_checkbox checkbox;
		struct widget_data_info_text text;
	} info;
};

/* Display widget selected. */
void display_widget_focused(struct dialog_data *, struct widget_data *);
/* Display widget unselected. */
void display_widget_unfocused(struct dialog_data *, struct widget_data *);

void dlg_set_history(struct widget_data *);

static inline int
widget_is_focusable(struct widget_data *widget_data)
{
	switch (widget_data->widget->type) {
		case WIDGET_LISTBOX: return 0;
		case WIDGET_TEXT: return text_is_scrollable(widget_data);
		default: return 1;
	}
}


#endif
