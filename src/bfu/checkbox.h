/* $Id: checkbox.h,v 1.28 2003/12/27 03:27:31 miciah Exp $ */

#ifndef EL__BFU_CHECKBOX_H
#define EL__BFU_CHECKBOX_H

#include "bfu/style.h"
#include "bfu/widget.h"

struct terminal;

#define add_dlg_checkbox(dlg, text_, data_)				\
	do {								\
		int n = (dlg)->widgets_size;				\
		(dlg)->widgets[n].type = WIDGET_CHECKBOX;		\
		(dlg)->widgets[n].text = (text_);			\
		(dlg)->widgets[n].info.checkbox.gid = 0;		\
		(dlg)->widgets[n].info.checkbox.gnum = 0;		\
		(dlg)->widgets[n].datalen = sizeof(int);		\
		(dlg)->widgets[n].data = (unsigned char *) &(data_);	\
		(dlg)->widgets_size++;					\
	} while (0)

#define add_dlg_radio(dlg, text_, groupid, groupnum, data_)		\
	do {								\
		int n = (dlg)->widgets_size;				\
		(dlg)->widgets[n].type = WIDGET_CHECKBOX;		\
		(dlg)->widgets[n].text = (text_);			\
		(dlg)->widgets[n].info.checkbox.gid = (groupid);	\
		(dlg)->widgets[n].info.checkbox.gnum = (groupnum);	\
		(dlg)->widgets[n].datalen = sizeof(int);		\
		(dlg)->widgets[n].data = (unsigned char *) &(data_);	\
		(dlg)->widgets_size++;					\
	} while (0)

extern struct widget_ops checkbox_ops;
void
dlg_format_checkbox(struct terminal *term,
		    struct widget_data *widget_data,
		    int x, int *y, int w, int *rw,
		    enum format_align align);

#endif
