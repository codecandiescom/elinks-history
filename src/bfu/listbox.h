/* $Id: listbox.h,v 1.24 2003/04/29 13:18:22 zas Exp $ */

#ifndef EL__BFU_LISTBOX_H
#define EL__BFU_LISTBOX_H

#include "bfu/align.h"
#include "bfu/dialog.h"
#include "bfu/menu.h"
#include "bfu/widget.h"
#include "lowlevel/terminal.h"
#include "util/lists.h"


struct listbox_data;
struct listbox_item;

struct listbox_ops {
	void (*del)(struct terminal *, struct listbox_data *);
};

/* Stores display information about a box. Kept in cdata. */
struct listbox_data {
	LIST_HEAD(struct listbox_data);

	struct listbox_ops *ops; /* Backend-provided operations */

	struct listbox_item *sel; /* Item currently selected */
	struct listbox_item *top; /* Item which is on the top line of the box */
	int sel_offset; /* Offset of selected item against the box top */
	struct list_head *items; /* The list being displayed */
};

/* An item in a box */
struct listbox_item {
	LIST_HEAD(struct listbox_item);

	/* These may be NULL/empty list for root/leaf nodes or non-hiearchic
	 * listboxes. */
	struct listbox_item *root;
	struct list_head child;
	enum { BI_LEAF, BI_FOLDER } type;
	int expanded:1; /* Only valid if this is a BI_FOLDER */
	int visible:1; /* Is this item visible? */
	int marked:1;
	int translated:1; /* Should we call gettext on this text? */
	int depth;

	/* Run when this item is hilighted */
	void (*on_hilight)(struct terminal *, struct listbox_data *, struct listbox_item *);
	/* Run when the user selects on this item. Returns pointer to the
	 * listbox_item that should be selected after execution. */
	int (*on_selected)(struct terminal *, struct listbox_data *, struct listbox_item *);
	struct list_head *box;
	void *udata;
	enum item_free item_free;

	/* Text to display (must be last) */
	unsigned char *text;
};

extern struct widget_ops listbox_ops;

void dlg_format_box(struct terminal *, struct terminal *, struct widget_data *, int, int *, int, int *, enum format_align);

struct listbox_item *traverse_listbox_items_list(struct listbox_item *, int, int, int (*)(struct listbox_item *, void *, int *), void *);

void box_sel_move(struct widget_data *, int);

#endif
