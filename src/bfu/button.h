/* $Id: button.h,v 1.36 2005/03/05 20:14:24 zas Exp $ */

#ifndef EL__BFU_BUTTON_H
#define EL__BFU_BUTTON_H

#include "bfu/common.h"
#include "util/align.h"

struct dialog;
struct terminal;
struct widget_data;

typedef void (done_handler_T)(void *);

struct widget_info_button {
	int flags;
	/* Used by some default handlers like ok_dialog()
	 * as a callback. */
	done_handler_T *done;
	void *done_data;
};

/* Button flags, go into widget.gid */
#define B_ENTER		1
#define B_ESC		2

void add_dlg_button_do(struct dialog *dlg, unsigned char *text, int flags, t_widget_handler *handler, void *data, done_handler_T *done, void *done_data);

#define add_dlg_ok_button(dlg, text, flags, done, data)	\
	add_dlg_button_do(dlg, text, flags, ok_dialog, NULL, done, data)

#define add_dlg_button(dlg, text, flags, handler, data)	\
	add_dlg_button_do(dlg, text, flags, handler, data, NULL, NULL)

extern struct widget_ops button_ops;
void dlg_format_buttons(struct terminal *, struct widget_data *, int, int, int *, int, int *, enum format_align);

#endif
