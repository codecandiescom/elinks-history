/* $Id: widget.h,v 1.6 2002/07/09 23:01:07 pasky Exp $ */

#ifndef EL__BFU_WIDGET_H
#define EL__BFU_WIDGET_H

#include "bfu/inphist.h"
#include "lowlevel/terminal.h"
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

struct widget_ops {
	void (*display)(struct widget_data *, struct dialog_data *, int);
	void (*init)(struct widget_data *, struct dialog_data *, struct event *);
	int (*mouse)(struct widget_data *, struct dialog_data *, struct event *);
};

struct widget {
	enum widget_type type;
	struct widget_ops *ops;

	/* for buttons: gid - flags B_XXX
	 * for fields:  min/max
	 * for box:     gid is box height */
	int gid, gnum;
	struct input_history *history;
	/* void *widget_data; */

	void *udata;

	int (*fn)(struct dialog_data *, struct widget_data *);

	int dlen;
	unsigned char *data;
	unsigned char *text;
};

struct widget_data {
	int x, y, l;
	int vpos, cpos;
	int checked;
	struct widget *item;
	struct list_head history;
	struct input_history_item *cur_hist;
	unsigned char *cdata;
};

void display_dlg_item(struct dialog_data *, struct widget_data *, int);

void dlg_select_item(struct dialog_data *, struct widget_data *);
void dlg_set_history(struct widget_data *);

#endif
