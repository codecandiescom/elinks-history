/* $Id: widget.h,v 1.19 2003/10/26 15:19:32 zas Exp $ */

#ifndef EL__BFU_WIDGET_H
#define EL__BFU_WIDGET_H

#include "bfu/inphist.h"
#include "terminal/terminal.h"
#include "util/lists.h"


struct widget_data;
struct dialog_data; /* XXX */

enum widget_type {
	D_END,
	D_CHECKBOX,
	D_FIELD,
	D_FIELD_PASS,
	D_BUTTON,
	D_BOX,
};

#define add_dlg_end(dlg, n)						\
	do {								\
		(dlg)->widgets[n].type = D_END;				\
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

	struct input_history *history;

	unsigned char *text;
	unsigned char *data;

	void *udata;

	int (*fn)(struct dialog_data *, struct widget_data *);

	union {
		struct checkbox_info {
			int gid;
			int gnum;
		} checkbox;
		struct field_info {
			int min;
			int max;
		} field;
		struct box_info {
			int height;
		} box;
		struct button_info {
			int flags;
		} button;
	} info;
		
	int dlen;

	enum widget_type type;
};

struct widget_data {
	struct list_head history;

	struct widget *widget;
	struct input_history_item *cur_hist;

	unsigned char *cdata;

	int x, y;
	int l, h;
	int vpos, cpos;
	int checked;
};

void display_dlg_item(struct dialog_data *, struct widget_data *, int);

void dlg_set_history(struct widget_data *);

#define selected_widget(dlg_data) (&(dlg_data)->widgets_data[(dlg_data)->selected])

#endif
