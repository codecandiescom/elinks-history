/* $Id: widget.h,v 1.48 2003/12/22 04:30:37 miciah Exp $ */

#ifndef EL__BFU_WIDGET_H
#define EL__BFU_WIDGET_H

#include "bfu/style.h"
#include "util/lists.h"

struct dialog_data;
struct input_history;
struct input_history_entry;
struct term_event;

struct widget_data;

enum widget_type {
	WIDGET_CHECKBOX,
	WIDGET_FIELD,
	WIDGET_FIELD_PASS,
	WIDGET_BUTTON,
	WIDGET_LISTBOX,
	WIDGET_TEXT,
};

#define add_dlg_end(dlg, n)						\
	do {								\
		assert(n == (dlg)->widgets_size);			\
	} while (0)

struct widget_ops {
	/* XXX: Order matters here. --Zas */
	void (*display)(struct widget_data *, struct dialog_data *, int);
	void (*init)(struct widget_data *, struct dialog_data *, struct term_event *);
	int (*mouse)(struct widget_data *, struct dialog_data *, struct term_event *);
	int (*kbd)(struct widget_data *, struct dialog_data *, struct term_event *);
	void (*select)(struct widget_data *, struct dialog_data *);
};

struct widget {
	struct widget_ops *ops;

	unsigned char *text;
	unsigned char *data;

	void *udata;

	int (*fn)(struct dialog_data *, struct widget_data *);

	union {
		struct {
			int gid;
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

	int x, y;
	int w, h;

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
		} text;
	} info;
};

void display_dlg_item(struct dialog_data *, struct widget_data *, int);

void dlg_set_history(struct widget_data *);

#define selected_widget(dlg_data) (&(dlg_data)->widgets_data[(dlg_data)->selected])

#define widget_has_history(widget_data) ((widget_data)->widget->type == WIDGET_FIELD \
					 && (widget_data)->widget->info.field.history)

#define widget_is_textfield(widget_data) ((widget_data)->widget->type == WIDGET_FIELD \
					  || (widget_data)->widget->type == WIDGET_FIELD_PASS)

#define text_is_scrollable(widget_data) \
	((widget_data)->widget->info.text.is_scrollable \
	 && (widget_data)->h < (widget_data)->info.text.lines)

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
