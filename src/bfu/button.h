/* $Id: button.h,v 1.26 2004/07/02 16:00:46 zas Exp $ */

#ifndef EL__BFU_BUTTON_H
#define EL__BFU_BUTTON_H

#include "bfu/style.h"
#include "bfu/widget.h"

struct terminal;

/* Button flags, go into widget.gid */
#define B_ENTER		1
#define B_ESC		2

#define add_dlg_button_do(dlg, key, handler, text_, data_, done_, done_data_)\
	do {								\
		struct widget *widget;					\
									\
		widget = &(dlg)->widgets[(dlg)->widgets_size++];	\
		widget->type = WIDGET_BUTTON;				\
		widget->info.button.flags = (key);			\
		widget->info.button.done = (void (*)(void *)) (done_);	\
		widget->info.button.done_data = (done_data_);		\
		widget->fn = (handler);					\
		widget->text = (text_);					\
		widget->udata = (data_);				\
	} while (0)

#define add_dlg_ok_button(dlg, key, text, done, data)	\
	add_dlg_button_do(dlg, key, ok_dialog, text, NULL, done, data)

#define add_dlg_button(dlg, key, handler, text, data)	\
	add_dlg_button_do(dlg, key, handler, text, data, NULL, NULL)

extern struct widget_ops button_ops;
void dlg_format_buttons(struct terminal *, struct widget_data *, int, int, int *, int, int *, enum format_align);

#endif
