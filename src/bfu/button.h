/* $Id: button.h,v 1.14 2003/10/26 14:04:09 zas Exp $ */

#ifndef EL__BFU_BUTTON_H
#define EL__BFU_BUTTON_H

#include "bfu/style.h"
#include "bfu/widget.h"
#include "terminal/terminal.h"

/* Button flags, go into widget.gid */
#define B_ENTER		1
#define B_ESC		2

#define add_dlg_button(dlg, n, key, handler, button_text, button_data)	\
	do {								\
		(dlg)->widgets[n].type = D_BUTTON;			\
		(dlg)->widgets[n].gid = (key);				\
		(dlg)->widgets[n].fn = (handler);				\
		(dlg)->widgets[n].text = (button_text);			\
		(dlg)->widgets[n].udata = (button_data);			\
		(n)++;							\
	} while (0)

extern struct widget_ops button_ops;
void buttons_width(struct terminal *term, struct widget_data *widget_data, int n, int *minwidth, int *maxwidth);
void max_buttons_width(struct terminal *, struct widget_data *, int, int *);
void min_buttons_width(struct terminal *, struct widget_data *, int, int *);
void dlg_format_buttons(struct terminal *, struct terminal *, struct widget_data *, int, int, int *, int, int *, enum format_align);

#endif
