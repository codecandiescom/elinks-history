/* $Id: dialog.h,v 1.14 2003/11/05 09:23:18 zas Exp $ */

#ifndef EL__BFU_DIALOG_H
#define EL__BFU_DIALOG_H

#include "bfu/style.h"
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
	int (*handle_event)(struct dialog_data *, struct term_event *);
	void (*abort)(struct dialog_data *);
	void (*refresh)(void *);

	enum format_align align;

	size_t widgets_size;
	struct widget widgets[1]; /* must be at end of struct */
};

/* Allocate a struct dialog for n - 1 widgets, one is already reserved in struct.
 * add_size bytes will be added. */
#define sizeof_dialog(n, add_size) \
	(sizeof(struct dialog) + ((n) - 1) * sizeof(struct widget) + (add_size))

#define calloc_dialog(n, add_size) ((struct dialog *) mem_calloc(1, sizeof_dialog(n, add_size)))

struct dialog_data {
	struct window *win;
	struct dialog *dlg;
	struct memory_list *ml;

	int x, y;
	int width, height;
	int n;
	int selected;

	struct widget_data widgets_data[1]; /* must be at end of struct */
};


struct dialog_data *do_dialog(struct terminal *, struct dialog *,
			      struct memory_list *);

void dialog_func(struct window *, struct term_event *, int);

void center_dlg(struct dialog_data *);
void draw_dlg(struct dialog_data *);

int ok_dialog(struct dialog_data *, struct widget_data *);
int cancel_dialog(struct dialog_data *, struct widget_data *);
int clear_dialog(struct dialog_data *, struct widget_data *);
int check_dialog(struct dialog_data *);
int update_dialog_data(struct dialog_data *, struct widget_data *);

#endif
