/* $Id: checkbox.h,v 1.34 2004/11/19 15:33:07 zas Exp $ */

#ifndef EL__BFU_CHECKBOX_H
#define EL__BFU_CHECKBOX_H

#include "bfu/style.h"

struct dialog;
struct terminal;
struct widget_data;

void add_dlg_radio_do(struct dialog *dlg, unsigned char *text, int groupid, int groupnum, void *data);

#define add_dlg_radio(dlg, text_, groupid, groupnum, data_) \
	add_dlg_radio_do(dlg, text_, groupid, groupnum, (void *) &(data_))

#define add_dlg_checkbox(dlg, text_, data_) \
	add_dlg_radio_do(dlg, text_, 0, 0, (void *) &(data_))

extern struct widget_ops checkbox_ops;

void
dlg_format_checkbox(struct terminal *term,
		    struct widget_data *widget_data,
		    int x, int *y, int w, int *rw,
		    enum format_align align);

#endif
