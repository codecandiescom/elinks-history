/* $Id: button.h,v 1.21 2003/11/07 18:45:39 jonas Exp $ */

#ifndef EL__BFU_BUTTON_H
#define EL__BFU_BUTTON_H

#include "bfu/style.h"
#include "bfu/widget.h"
#include "terminal/terminal.h"

/* Button flags, go into widget.gid */
#define B_ENTER		1
#define B_ESC		2

#define add_dlg_button(dlg, key, handler, button_text, button_data)	\
	do {								\
		int n = (dlg)->widgets_size;				\
		(dlg)->widgets[n].type = WIDGET_BUTTON;			\
		(dlg)->widgets[n].info.button.flags = (key);		\
		(dlg)->widgets[n].fn = (handler);			\
		(dlg)->widgets[n].text = (button_text);			\
		(dlg)->widgets[n].udata = (button_data);		\
		(dlg)->widgets_size++;					\
	} while (0)

extern struct widget_ops button_ops;
void dlg_format_buttons(struct terminal *, struct widget_data *, int, int, int *, int, int *, enum format_align);

#endif
