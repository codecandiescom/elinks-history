/* $Id: button.h,v 1.10 2003/10/24 22:46:03 pasky Exp $ */

#ifndef EL__BFU_BUTTON_H
#define EL__BFU_BUTTON_H

#include "bfu/style.h"
#include "bfu/widget.h"
#include "terminal/terminal.h"

/* Button flags, go into widget.gid */
#define B_ENTER		1
#define B_ESC		2

#define set_dlg_button(dlg, n, key, handler, button_text, udata)	\
	do {								\
		(dlg)->items[n].type = D_BUTTON;			\
		(dlg)->items[n].gid = (key);				\
		(dlg)->items[n].fn = (handler);				\
		(dlg)->items[n].text = (button_text);			\
		(dlg)->items[n].udata = (udata);			\
		(n)++;						\
	} while (0)

extern struct widget_ops button_ops;
void buttons_width(struct terminal *term, struct widget_data *butt, int n, int *minwidth, int *maxwidth);
void max_buttons_width(struct terminal *, struct widget_data *, int, int *);
void min_buttons_width(struct terminal *, struct widget_data *, int, int *);
void dlg_format_buttons(struct terminal *, struct terminal *, struct widget_data *, int, int, int *, int, int *, enum format_align);

#endif
