/* $Id: button.h,v 1.33 2004/11/21 14:07:00 zas Exp $ */

#ifndef EL__BFU_BUTTON_H
#define EL__BFU_BUTTON_H

#include "util/align.h"

struct dialog;
struct terminal;
struct widget_data;

typedef void (t_done_handler)(void *);

struct widget_info_button {
	int flags;
	/* Used by some default handlers like ok_dialog()
	 * as a callback. */
	t_done_handler *done;
	void *done_data;
};

/* Button flags, go into widget.gid */
#define B_ENTER		1
#define B_ESC		2

void add_dlg_button_do(struct dialog *dlg, int flags, void *handler, unsigned char *text, void *data, t_done_handler *done, void *done_data);

#define add_dlg_ok_button(dlg, flags, text, done, data)	\
	add_dlg_button_do(dlg, flags, ok_dialog, text, NULL, done, data)

#define add_dlg_button(dlg, flags, handler, text, data)	\
	add_dlg_button_do(dlg, flags, handler, text, data, NULL, NULL)

extern struct widget_ops button_ops;
void dlg_format_buttons(struct terminal *, struct widget_data *, int, int, int *, int, int *, enum format_align);

#endif
