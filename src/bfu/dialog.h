/* $Id: dialog.h,v 1.5 2003/05/07 10:39:04 zas Exp $ */

#ifndef EL__BFU_DIALOG_H
#define EL__BFU_DIALOG_H

#include "bfu/align.h"
#include "bfu/widget.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/memlist.h"


struct dialog_data;

/* Event handlers return this values */
#define	EVENT_PROCESSED		0
#define EVENT_NOT_PROCESSED	1

struct dialog {
	unsigned char *title;
	void *udata;
	void *udata2;
	void *refresh_data;

	void (*fn)(struct dialog_data *);
	int (*handle_event)(struct dialog_data *, struct event *);
	void (*abort)(struct dialog_data *);
	void (*refresh)(void *);

	enum format_align align;

	struct widget items[1]; /* must be at end of struct */
};

struct dialog_data {
	struct window *win;
	struct dialog *dlg;
	struct memory_list *ml;

	int x, y;
	int xw, yw;
	int n;
	int selected;

	struct widget_data items[1]; /* must be at end of struct */
};


struct dialog_data *do_dialog(struct terminal *, struct dialog *,
			      struct memory_list *);

void dialog_func(struct window *, struct event *, int);

void center_dlg(struct dialog_data *);
void draw_dlg(struct dialog_data *);

int ok_dialog(struct dialog_data *, struct widget_data *);
int cancel_dialog(struct dialog_data *, struct widget_data *);
int clear_dialog(struct dialog_data *, struct widget_data *);
int check_dialog(struct dialog_data *);
int update_dialog_data(struct dialog_data *, struct widget_data *);

#endif
