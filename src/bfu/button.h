/* $Id: button.h,v 1.24 2004/06/22 06:46:15 miciah Exp $ */

#ifndef EL__BFU_BUTTON_H
#define EL__BFU_BUTTON_H

#include "bfu/style.h"
#include "bfu/widget.h"

struct terminal;

/* Button flags, go into widget.gid */
#define B_ENTER		1
#define B_ESC		2

#define add_dlg_button_do(dlg, key, handler, text_, data_, done_, done_data_)	\
	do {								\
		int n = (dlg)->widgets_size;				\
		(dlg)->widgets[n].type = WIDGET_BUTTON;			\
		(dlg)->widgets[n].info.button.flags = (key);		\
		(dlg)->widgets[n].info.button.done = (void (*)(void *)) (done_); \
		(dlg)->widgets[n].info.button.done_data = (done_data_);	\
		(dlg)->widgets[n].fn = (handler);			\
		(dlg)->widgets[n].text = (text_);			\
		(dlg)->widgets[n].udata = (data_);			\
		(dlg)->widgets_size++;					\
	} while (0)

#define add_dlg_ok_button(dlg, key, text, done, data)	\
	add_dlg_button_do(dlg, key, ok_dialog, text, NULL, done, data)

#define add_dlg_button(dlg, key, handler, text, data)	\
	add_dlg_button_do(dlg, key, handler, text, data, NULL, NULL)

extern struct widget_ops button_ops;
void dlg_format_buttons(struct terminal *, struct widget_data *, int, int, int *, int, int *, enum format_align);

#endif
