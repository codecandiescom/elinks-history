/* $Id: widget.h,v 1.26 2003/10/28 19:03:56 jonas Exp $ */

#ifndef EL__BFU_WIDGET_H
#define EL__BFU_WIDGET_H

#include "bfu/inphist.h"
#include "terminal/terminal.h"
#include "util/lists.h"


struct widget_data;
struct dialog_data; /* XXX */

enum widget_type {
	WIDGET_END,
	WIDGET_CHECKBOX,
	WIDGET_FIELD,
	WIDGET_FIELD_PASS,
	WIDGET_BUTTON,
	WIDGET_LISTBOX,
};

#define add_dlg_end(dlg, n)						\
	do {								\
		(dlg)->widgets[n].type = WIDGET_END;			\
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
		} field;
		struct {
			int height;
		} box;
		struct {
			int flags;
		} button;
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
			struct input_history_item *cur_hist;
		} field;
		struct {
			int checked;
		} checkbox;
	} info;
};

void display_dlg_item(struct dialog_data *, struct widget_data *, int);

void dlg_set_history(struct widget_data *);

#define selected_widget(dlg_data) (&(dlg_data)->widgets_data[(dlg_data)->selected])

#define widget_has_history(widget_data) ((widget_data)->widget->type == WIDGET_FIELD \
					 && (widget_data)->widget->info.field.history)

#define widget_is_textfield(widget_data) ((widget_data)->widget->type == WIDGET_FIELD \
					  || (widget_data)->widget->type == WIDGET_FIELD_PASS)

#endif
