/* $Id: widget.h,v 1.1 2002/07/04 21:04:45 pasky Exp $ */

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

struct widget {
	enum widget_type type;
	/* for buttons:	gid - flags B_XXX
	 * for fields:	min/max
	 * for box:	gid is box height */
	int gid, gnum;
	int (*fn)(struct dialog_data *, struct widget_data *);
	struct input_history *history;
	int dlen;
	unsigned char *data;
	/* for box:	holds list */
	void *udata;
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

#endif
