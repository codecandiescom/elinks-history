/* $Id: bfu.h,v 1.14 2002/07/04 15:45:38 pasky Exp $ */

#ifndef EL__BFU_BFU_H
#define EL__BFU_BFU_H

#include "bfu/align.h"
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

/* Event handlers return this values */
#define	EVENT_PROCESSED		0
#define EVENT_NOT_PROCESSED	1

struct dialog {
	unsigned char *title;
	void (*fn)(struct dialog_data *);
	int (*handle_event)(struct dialog_data *, struct event *);
	void (*abort)(struct dialog_data *);
	void *udata;
	void *udata2;
	enum format_align align;
	void (*refresh)(void *);
	void *refresh_data;
	struct widget items[1];
};

struct dialog_data {
	struct window *win;
	struct dialog *dlg;
	int x, y, xw, yw;
	int n;
	int selected;
	struct memory_list *ml;
	struct widget_data items[1];
};


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


struct dialog_data *do_dialog(struct terminal *, struct dialog *,
			      struct memory_list *);

void dialog_func(struct window *, struct event *, int);

void dlg_format_box(struct terminal *, struct terminal *, struct widget_data *, int, int *, int, int *, enum format_align);

void center_dlg(struct dialog_data *);
void draw_dlg(struct dialog_data *);

void display_dlg_item(struct dialog_data *, struct widget_data *, int);

int ok_dialog(struct dialog_data *, struct widget_data *);
int cancel_dialog(struct dialog_data *, struct widget_data *);
int clear_dialog(struct dialog_data *, struct widget_data *);
int check_dialog(struct dialog_data *);

void box_sel_move(struct widget_data *, int );
void show_dlg_item_box(struct dialog_data *, struct widget_data *);
void box_sel_set_visible(struct widget_data *, int );

#endif
