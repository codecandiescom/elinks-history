/* $Id: listbox.h,v 1.2 2002/07/04 21:19:44 pasky Exp $ */

#ifndef EL__BFU_LISTBOX_H
#define EL__BFU_LISTBOX_H

#include "bfu/align.h"
#include "bfu/dialog.h"
#include "lowlevel/terminal.h"
#include "util/lists.h"


/* Stores display information about a box. Kept in cdata. */
struct dlg_data_item_data_box {
	int sel;	/* Item currently selected */
	int box_top;	/* Index into items of the item that is on the top
			   line of the box */
	struct list_head items;	/* The list being displayed */
	int list_len;	/* Number of items in the list */
};

/* Which fields to free when zapping a box_item. Bitwise or these. */
enum box_item_free {
	NOTHING,
	TEXT,
	DATA
};

/* An item in a box */
struct box_item {
	struct box_item *next;
	struct box_item *prev;
	/* Text to display */
	unsigned char *text;
	/* Run when this item is hilighted */
	void (*on_hilight)(struct terminal *, struct dlg_data_item_data_box *, struct box_item *);
	/* Run when the user selects on this item. Returns pointer to the
	 * box_item that should be selected after execution. */
	int (*on_selected)(struct terminal *, struct dlg_data_item_data_box *, struct box_item *);
	void *data;
	enum box_item_free free_i;
};


void dlg_format_box(struct terminal *, struct terminal *, struct widget_data *, int, int *, int, int *, enum format_align);

void box_sel_move(struct widget_data *, int);
void show_dlg_item_box(struct dialog_data *, struct widget_data *);
void box_sel_set_visible(struct widget_data *, int);

#endif
