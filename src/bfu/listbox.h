/* $Id: listbox.h,v 1.6 2002/08/11 18:54:23 pasky Exp $ */

#ifndef EL__BFU_LISTBOX_H
#define EL__BFU_LISTBOX_H

#include "bfu/align.h"
#include "bfu/dialog.h"
#include "bfu/menu.h"
#include "bfu/widget.h"
#include "lowlevel/terminal.h"
#include "util/lists.h"


/* Stores display information about a box. Kept in cdata. */
struct listbox_data {
	int sel;	/* Item currently selected */
	int box_top;	/* Index into items of the item that is on the top
			   line of the box */
	struct list_head items;	/* The list being displayed */
	int list_len;	/* Number of items in the list */
};

/* An item in a box */
struct listbox_item {
	struct listbox_item *next;
	struct listbox_item *prev;

	/* These may be NULL for root/leaf nodes or non-hiearchic listboxes. */
	struct listbox_item *child;
	struct listbox_item *parent;

	/* Text to display */
	unsigned char *text;
	/* Run when this item is hilighted */
	void (*on_hilight)(struct terminal *, struct listbox_data *, struct listbox_item *);
	/* Run when the user selects on this item. Returns pointer to the
	 * listbox_item that should be selected after execution. */
	int (*on_selected)(struct terminal *, struct listbox_data *, struct listbox_item *);
	void *data;
	enum item_free item_free;
};

extern struct widget_ops listbox_ops;

void dlg_format_box(struct terminal *, struct terminal *, struct widget_data *, int, int *, int, int *, enum format_align);

void box_sel_move(struct widget_data *, int);
void box_sel_set_visible(struct widget_data *, int);

#endif
