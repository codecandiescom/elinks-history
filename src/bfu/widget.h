/* $Id: widget.h,v 1.44 2003/11/29 01:46:26 jonas Exp $ */

#ifndef EL__BFU_WIDGET_H
#define EL__BFU_WIDGET_H

#include "bfu/inphist.h"
#include "bfu/style.h"
#include "terminal/terminal.h"
#include "util/lists.h"


struct widget_data;
struct dialog_data; /* XXX */

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
			/* Used only for the scrollable text widget */
			int current;

			/* The number of lines saved in @cdata */
			int lines;

			/* The maximum dialog width the lines are valid for */
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

static inline int
widget_is_focusable(struct widget_data *widget_data)
{
	enum widget_type type = widget_data->widget->type;

	if (type == WIDGET_LISTBOX) return 0;
	if (type != WIDGET_TEXT) return 1;

	/* Only focus if there is some text to scroll */
	return (widget_data->widget->info.text.is_scrollable
		&& widget_data->h < widget_data->info.text.lines);
}

#define widget_has_group(widget_data)	((widget_data)->widget->type == WIDGET_CHECKBOX \
					  ? (widget_data)->widget->info.checkbox.gid : -1)

#endif
