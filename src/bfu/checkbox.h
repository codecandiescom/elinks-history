/* $Id: checkbox.h,v 1.20 2003/11/04 23:25:42 jonas Exp $ */

#ifndef EL__BFU_CHECKBOX_H
#define EL__BFU_CHECKBOX_H

#include "bfu/widget.h"
#include "terminal/terminal.h"

#define add_dlg_checkbox(dlg, n, text_, data_)				\
	do {								\
		(dlg)->widgets[n].type = WIDGET_CHECKBOX;		\
		(dlg)->widgets[n].text = (text_);			\
		(dlg)->widgets[n].info.checkbox.gid = 0;		\
		(dlg)->widgets[n].info.checkbox.gnum = 0;		\
		(dlg)->widgets[n].datalen = sizeof(int);		\
		(dlg)->widgets[n].data = (unsigned char *) &(data_);	\
		(n)++;							\
	} while (0)

 #define add_dlg_radio(dlg, n, text_, groupid, groupnum, data_)		\
	do {								\
		(dlg)->widgets[n].type = WIDGET_CHECKBOX;		\
		(dlg)->widgets[n].text = (text_);			\
		(dlg)->widgets[n].info.checkbox.gid = (groupid);	\
		(dlg)->widgets[n].info.checkbox.gnum = (groupnum);	\
		(dlg)->widgets[n].datalen = sizeof(int);		\
		(dlg)->widgets[n].data = (unsigned char *) &(data_);	\
		(n)++;							\
	} while (0)

extern struct widget_ops checkbox_ops;
void checkboxes_width(struct terminal *term, struct widget_data *widget_data, int n, int *minwidth, int *maxwidth);
void dlg_format_checkboxes(struct terminal *, struct terminal *, struct widget_data *, int, int, int *, int, int *);

#endif
