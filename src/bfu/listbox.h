/* $Id: listbox.h,v 1.41 2003/11/09 11:40:14 jonas Exp $ */

#ifndef EL__BFU_LISTBOX_H
#define EL__BFU_LISTBOX_H

#include "bfu/dialog.h"
#include "bfu/menu.h"
#include "bfu/style.h"
#include "bfu/widget.h"
#include "terminal/terminal.h"
#include "util/lists.h"


#define add_dlg_listbox(dlg, height_, content)				\
	do {								\
		int n = (dlg)->widgets_size;				\
		(dlg)->widgets[n].type = WIDGET_LISTBOX;		\
		(dlg)->widgets[n].info.box.height = (height_);		\
		(dlg)->widgets[n].data = (void *) (content);		\
		(dlg)->widgets_size++;					\
	} while (0)


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

	/* The listbox this item is member of. */
	struct list_head *box;

	enum { BI_LEAF, BI_FOLDER } type;

	unsigned int expanded:1; /* Only valid if this is a BI_FOLDER */
	unsigned int visible:1; /* Is this item visible? */
	unsigned int marked:1;
	unsigned int translated:1; /* Should we call gettext on this text? */
	int depth;

	/* Run when this item is hilighted */
	void (*on_hilight)(struct terminal *, struct listbox_data *, struct listbox_item *);
	/* Run when the user selects on this item. Returns pointer to the
	 * listbox_item that should be selected after execution. */
	int (*on_selected)(struct terminal *, struct listbox_data *, struct listbox_item *);

	void *udata;

	enum item_free item_free;

	/* Text to display (must be last) */
	unsigned char *text;
};

extern struct widget_ops listbox_ops;

void dlg_format_box(struct terminal *, struct widget_data *, int, int *, int, int, int *, enum format_align);

struct listbox_item *traverse_listbox_items_list(struct listbox_item *, int, int, int (*)(struct listbox_item *, void *, int *), void *);

void box_sel_move(struct widget_data *, int);

static inline struct listbox_data *
get_listbox_widget_data(struct widget_data *widget_data)
{
	assert(widget_data->widget->type == WIDGET_LISTBOX);
	return ((struct listbox_data *) widget_data->widget->data);
}

#define get_dlg_listbox_data(dlg_data) \
	get_listbox_widget_data(dlg_data->widgets_data)

#endif
