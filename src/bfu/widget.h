/* $Id: widget.h,v 1.76 2004/11/19 16:33:01 zas Exp $ */

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
#include "bfu/listbox.h"
#include "bfu/style.h"
#include "bfu/text.h"
#include "util/lists.h"
#include "util/box.h"

struct dialog_data;
struct input_history;
struct input_history_entry;
struct term_event;

struct widget_data;


#define add_dlg_end(dlg, n)						\
	do {								\
		assert(n == (dlg)->number_of_widgets);			\
	} while (0)


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
		struct {
			/* gid is 0 for checkboxes, or a positive int for
			 * each group of radio buttons. */
			int gid;
			/* gnum is 0 for checkboxes, or a positive int for
			 * each radio button in a group. */
			int gnum;
		} checkbox;
		struct {
			int min;
			int max;
			struct input_history *history;
			int float_label;
		} field;
		struct {
			int height;
		} box;
		struct {
			int flags;
			/* Used by some default handlers like ok_dialog()
			 * as a callback. */
			void (*done)(void *);
			void *done_data;
		} button;
		struct {
			enum format_align align;
			unsigned int is_label:1;
			unsigned int is_scrollable:1;
		} text;
	} info;

	int datalen;

	enum widget_type type;
};

struct widget_data {
	struct widget *widget;
	unsigned char *cdata;

	struct box box;

	union {
		struct {
			int vpos;
			int cpos;
			struct list_head history;
			struct input_history_entry *cur_hist;
		} field;
		struct {
			int checked;
		} checkbox;
		struct {
			/* The number of the first line that should be
			 * displayed within the widget.
			 * This is used only for scrollable text widgets */
			int current;

			/* The number of lines saved in @cdata */
			int lines;

			/* The dialog width to which the lines are wrapped.
			 * This is used to check whether the lines must be
			 * rewrapped. */
			int max_width;
#ifdef CONFIG_MOUSE
			/* For mouse scrollbar handling. See bfu/text.c.*/

			/* Height of selected part of scrollbar. */
			int scroller_height;

			/* Position of selected part of scrollbar. */
			int scroller_y;

			/* Direction of last mouse scroll. Used to adjust
			 * scrolling when selected bar part has a low height
			 * (especially the 1 char height) */
			int scroller_last_dir;
#endif
		} text;
	} info;
};

/* Display widget selected. */
void display_widget_focused(struct dialog_data *, struct widget_data *);
/* Display widget unselected. */
void display_widget_unfocused(struct dialog_data *, struct widget_data *);

void dlg_set_history(struct widget_data *);


#define widget_has_history(widget_data) ((widget_data)->widget->type == WIDGET_FIELD \
					 && (widget_data)->widget->info.field.history)

#define widget_is_textfield(widget_data) ((widget_data)->widget->type == WIDGET_FIELD \
					  || (widget_data)->widget->type == WIDGET_FIELD_PASS)

#define text_is_scrollable(widget_data) \
	((widget_data)->widget->info.text.is_scrollable \
	 && (widget_data)->box.height > 0 \
	 && (widget_data)->info.text.lines > 0 \
	 && (widget_data)->box.height < (widget_data)->info.text.lines)

static inline int
widget_is_focusable(struct widget_data *widget_data)
{
	switch (widget_data->widget->type) {
		case WIDGET_LISTBOX: return 0;
		case WIDGET_TEXT: return text_is_scrollable(widget_data);
		default: return 1;
	}
}

#define widget_has_group(widget_data)	((widget_data)->widget->type == WIDGET_CHECKBOX \
					  ? (widget_data)->widget->info.checkbox.gid : -1)

#endif
