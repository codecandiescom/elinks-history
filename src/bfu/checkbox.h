/* $Id: checkbox.h,v 1.14 2003/10/28 19:03:56 jonas Exp $ */

#ifndef EL__BFU_CHECKBOX_H
#define EL__BFU_CHECKBOX_H

#include "bfu/dialog.h"
#include "bfu/widget.h"
#include "terminal/terminal.h"

#define add_dlg_checkbox(dlg, n, groupid, groupnum, data_)		\
	do {								\
		(dlg)->widgets[n].type = WIDGET_CHECKBOX;		\
		(dlg)->widgets[n].info.checkbox.gid = (groupid);	\
		(dlg)->widgets[n].info.checkbox.gnum = (groupnum);	\
		(dlg)->widgets[n].datalen = sizeof(int);		\
		(dlg)->widgets[n].data = (unsigned char *) &(data_);	\
		(n)++;						\
	} while (0)

extern struct widget_ops checkbox_ops;
void checkboxes_width(struct terminal *term, int intl, unsigned char **texts, int *minwidth, int *maxwidth);
void dlg_format_checkboxes(struct terminal *, struct terminal *, int, struct widget_data *, int, int, int *, int, int *, unsigned char **);

void checkbox_list_fn(struct dialog_data *);

#endif
