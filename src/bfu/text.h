/* $Id: text.h,v 1.20 2004/07/02 15:51:38 zas Exp $ */

#ifndef EL__BFU_TEXT_H
#define EL__BFU_TEXT_H

#include "bfu/style.h"
#include "bfu/widget.h"
#include "util/color.h"

struct terminal;

#define add_dlg_text(dlg, text_, align_, bottom_pad_)			\
	do {								\
		struct widget *widget = &(dlg)->widgets[(dlg)->widgets_size++];\
		widget->type = WIDGET_TEXT;			\
		widget->text = (text_);				\
		widget->info.text.align = (align_);		\
		widget->info.text.is_label = (bottom_pad_);	\
	} while (0)

extern struct widget_ops text_ops;
void dlg_format_text_do(struct terminal *term,
		    unsigned char *text, int x, int *y, int w, int *rw,
		    struct color_pair *scolor, enum format_align align);

void
dlg_format_text(struct terminal *term, struct widget_data *widget_data,
		int x, int *y, int dlg_width, int *real_width, int height);

#endif
