/* $Id: bfu.h,v 1.2 2002/03/16 23:02:36 pasky Exp $ */

#ifndef EL__BFU_H
#define EL__BFU_H

#include "links.h" /* list_head */
#include "terminal.h"

struct memory_list {
	int n;
	void *p[1];
};

struct memory_list *getml(void *, ...);
void add_to_ml(struct memory_list **, ...);
void freeml(struct memory_list *);


#define MENU_FUNC (void (*)(struct terminal *, void *, void *))

extern unsigned char m_bar;
#define M_BAR	(&m_bar)

struct menu_item {
	unsigned char *text;
	unsigned char *rtext;
	unsigned char *hotkey;
	void (*func)(struct terminal *, void *, void *);
	void *data;
	int in_m;
	int free_i;
};

struct menu {
	int selected;
	int view;
	int xp, yp;
	int x, y, xw, yw;
	int ni;
	void *data;
	struct window *win;
	struct menu_item *items;
};

struct menu_item *new_menu(int);
void add_to_menu(struct menu_item **, unsigned char *, unsigned char *, unsigned char *, void (*)(struct terminal *, void *, void *), void *, int);
void do_menu(struct terminal *, struct menu_item *, void *);
void do_menu_selected(struct terminal *, struct menu_item *, void *, int);
void do_mainmenu(struct terminal *, struct menu_item *, void *, int);


struct history_item {
	struct history_item *next;
	struct history_item *prev;
	unsigned char d[1];
};

struct history {
	int n;
	struct list_head items;
};

void add_to_history(struct history *, unsigned char *, int);


struct dialog_item_data;
struct dialog_data;

enum dialog_item_type {
	D_END,
	D_CHECKBOX,
	D_FIELD,
	D_FIELD_PASS,
	D_BUTTON,
	D_BOX,
};

/* Button flags, go into dialog_item.gid */
#define B_ENTER		1
#define B_ESC		2

struct dialog_item {
	enum dialog_item_type type;
	/* for buttons:	gid - flags B_XXX
	 * for fields:	min/max
	 * for box:	gid is box height */
	int gid, gnum;
	int (*fn)(struct dialog_data *, struct dialog_item_data *);
	struct history *history;
	int dlen;
	unsigned char *data;
	/* for box:	holds list */
	void *udata;
	unsigned char *text;
};

struct dialog_item_data {
	int x, y, l;
	int vpos, cpos;
	int checked;
	struct dialog_item *item;
	struct list_head history;
	struct history_item *cur_hist;
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
	int align;
	void (*refresh)(void *);
	void *refresh_data;
	struct dialog_item items[1];
};

struct dialog_data {
	struct window *win;
	struct dialog *dlg;
	int x, y, xw, yw;
	int n;
	int selected;
	struct memory_list *ml;
	struct dialog_item_data items[1];
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

void show_dlg_item_box(struct dialog_data *, struct dialog_item_data *); 


void do_dialog(struct terminal *, struct dialog *, struct memory_list *);

int check_number(struct dialog_data *, struct dialog_item_data *);
int check_nonempty(struct dialog_data *, struct dialog_item_data *);

void max_text_width(struct terminal *, unsigned char *, int *);
void min_text_width(struct terminal *, unsigned char *, int *);
void dlg_format_text(struct terminal *, struct terminal *, unsigned char *, int, int *, int, int *, int, int);

void max_buttons_width(struct terminal *, struct dialog_item_data *, int, int *);
void min_buttons_width(struct terminal *, struct dialog_item_data *, int, int *);
void dlg_format_buttons(struct terminal *, struct terminal *, struct dialog_item_data *, int, int, int *, int, int *, int);

void checkboxes_width(struct terminal *, unsigned char **, int *, void (*)(struct terminal *, unsigned char *, int *));
void dlg_format_checkbox(struct terminal *, struct terminal *, struct dialog_item_data *, int, int *, int, int *, unsigned char *);
void dlg_format_checkboxes(struct terminal *, struct terminal *, struct dialog_item_data *, int, int, int *, int, int *, unsigned char **);

void dlg_format_field(struct terminal *, struct terminal *, struct dialog_item_data *, int, int *, int, int *, int);

void max_group_width(struct terminal *, unsigned char **, struct dialog_item_data *, int, int *);
void min_group_width(struct terminal *, unsigned char **, struct dialog_item_data *, int, int *);
void dlg_format_group(struct terminal *, struct terminal *, unsigned char **, struct dialog_item_data *, int, int, int *, int, int *);

void dlg_format_box(struct terminal *, struct terminal *, struct dialog_item_data *, int, int *, int, int *, int);

void checkbox_list_fn(struct dialog_data *);
void group_fn(struct dialog_data *);

void center_dlg(struct dialog_data *);
void draw_dlg(struct dialog_data *);

void display_dlg_item(struct dialog_data *, struct dialog_item_data *, int);

int ok_dialog(struct dialog_data *, struct dialog_item_data *);
int cancel_dialog(struct dialog_data *, struct dialog_item_data *);
int clear_dialog(struct dialog_data *, struct dialog_item_data *);

void msg_box(struct terminal *, struct memory_list *, unsigned char *, int, /*unsigned char *, void *, int,*/ ...);
	
void input_field_fn(struct dialog_data *);
void input_field(struct terminal *, struct memory_list *, unsigned char *, unsigned char *, unsigned char *, unsigned char *, void *, struct history *, int, unsigned char *, int, int, int (*)(struct dialog_data *, struct dialog_item_data *), void (*)(void *, unsigned char *), void (*)(void *));

void box_sel_move(struct dialog_item_data *, int ); 
void show_dlg_item_box(struct dialog_data *, struct dialog_item_data *);
void box_sel_set_visible(struct dialog_item_data *, int ); 

#endif
