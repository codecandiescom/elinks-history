/* $Id: text.h,v 1.11 2003/11/09 03:13:15 jonas Exp $ */

#ifndef EL__BFU_TEXT_H
#define EL__BFU_TEXT_H

#include "bfu/style.h"
#include "bfu/widget.h"
#include "terminal/terminal.h"
#include "util/color.h"

#define add_dlg_text(dlg, text)						\
	do {								\
		int n = (dlg)->widgets_size;				\
		(dlg)->widgets[n].type = WIDGET_TEXT;			\
		(dlg)->widgets[n].text = (text);			\
		(dlg)->widgets_size++;					\
	} while (0)

extern struct widget_ops text_ops;
void dlg_format_text(struct terminal *term,
		    unsigned char *text, int x, int *y, int w, int *rw,
		    struct color_pair *scolor, enum format_align align);

#endif
