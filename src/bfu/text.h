/* $Id: text.h,v 1.17 2003/11/28 19:38:00 jonas Exp $ */

#ifndef EL__BFU_TEXT_H
#define EL__BFU_TEXT_H

#include "bfu/style.h"
#include "bfu/widget.h"
#include "terminal/terminal.h"
#include "util/color.h"

#define add_dlg_text(dlg, text_, align_, bottom_pad_)			\
	do {								\
		int n = (dlg)->widgets_size;				\
		(dlg)->widgets[n].type = WIDGET_TEXT;			\
		(dlg)->widgets[n].text = (text_);			\
		(dlg)->widgets[n].info.text.align = (align_);		\
		(dlg)->widgets[n].info.text.is_label = (bottom_pad_);	\
		(dlg)->widgets_size++;					\
	} while (0)

extern struct widget_ops text_ops;
void dlg_format_text_do(struct terminal *term,
		    unsigned char *text, int x, int *y, int w, int *rw,
		    struct color_pair *scolor, enum format_align align);

void
dlg_format_text(struct terminal *term, struct widget_data *widget_data,
		int x, int *y, int dlg_width, int *real_width);

#endif
