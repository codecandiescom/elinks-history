/* $Id: checkbox.h,v 1.29 2004/05/26 16:35:58 zas Exp $ */

#ifndef EL__BFU_CHECKBOX_H
#define EL__BFU_CHECKBOX_H

#include "bfu/style.h"
#include "bfu/widget.h"

struct terminal;

#define add_dlg_radio(dlg, text_, groupid, groupnum, data_)	\
	do {							\
		int *n = &(dlg)->widgets_size;			\
		struct widget *widget = &(dlg)->widgets[*n];	\
								\
		widget->type = WIDGET_CHECKBOX;			\
		widget->text = (text_);				\
		widget->info.checkbox.gid = (groupid);		\
		widget->info.checkbox.gnum = (groupnum);	\
		widget->datalen = sizeof(int);			\
		widget->data = (unsigned char *) &(data_);	\
		(*n)++;						\
	} while (0)

#define add_dlg_checkbox(dlg, text_, data_) \
	add_dlg_radio(dlg, text_, 0, 0, data_)

extern struct widget_ops checkbox_ops;

void
dlg_format_checkbox(struct terminal *term,
		    struct widget_data *widget_data,
		    int x, int *y, int w, int *rw,
		    enum format_align align);

#endif
