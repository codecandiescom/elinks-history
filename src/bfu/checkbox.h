/* $Id: checkbox.h,v 1.9 2003/10/24 22:38:07 pasky Exp $ */

#ifndef EL__BFU_CHECKBOX_H
#define EL__BFU_CHECKBOX_H

#include "bfu/dialog.h"
#include "bfu/widget.h"
#include "terminal/terminal.h"

#define set_dlg_checkbox(dlg, n, groupid, groupnum, dataz)		\
	do {								\
		(dlg)->items[n].type = D_CHECKBOX;			\
		(dlg)->items[n].gid = (groupid);			\
		(dlg)->items[n].gnum = (groupnum);			\
		(dlg)->items[n].dlen = sizeof(int);			\
		(dlg)->items[n].data = (unsigned char *) &(dataz);	\
		(n)++;						\
	} while (0)

extern struct widget_ops checkbox_ops;
void checkboxes_width(struct terminal *term, int intl, unsigned char **texts, int *minwidth, int *maxwidth);
void dlg_format_checkboxes(struct terminal *, struct terminal *, int, struct widget_data *, int, int, int *, int, int *, unsigned char **);

void checkbox_list_fn(struct dialog_data *);

#endif
