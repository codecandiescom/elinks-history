/* $Id: button.h,v 1.29 2004/11/19 15:33:07 zas Exp $ */

#ifndef EL__BFU_BUTTON_H
#define EL__BFU_BUTTON_H

#include "bfu/style.h"

struct dialog;
struct terminal;
struct widget_data;

/* Button flags, go into widget.gid */
#define B_ENTER		1
#define B_ESC		2

#define BUTTON_DONE_FUNC(x) ((void (*)(void *))(x))

void add_dlg_button_do(struct dialog *dlg, int key, void *handler, unsigned char *text, void *data, void (*done)(void *), void *done_data);

#define add_dlg_ok_button(dlg, key, text, done, data)	\
	add_dlg_button_do(dlg, key, ok_dialog, text, NULL, BUTTON_DONE_FUNC(done), data)

#define add_dlg_button(dlg, key, handler, text, data)	\
	add_dlg_button_do(dlg, key, handler, text, data, BUTTON_DONE_FUNC(NULL), NULL)

extern struct widget_ops button_ops;
void dlg_format_buttons(struct terminal *, struct widget_data *, int, int, int *, int, int *, enum format_align);

#endif
