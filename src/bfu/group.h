/* $Id: group.h,v 1.14 2003/11/07 18:45:39 jonas Exp $ */

#ifndef EL__BFU_GROUP_H
#define EL__BFU_GROUP_H

#include "bfu/dialog.h"
#include "bfu/widget.h"
#include "terminal/terminal.h"

void dlg_format_group(struct terminal *term,
		 struct widget_data *widget_data,
		 int n, int x, int *y, int w, int *rw);

void group_layouter(struct dialog_data *);

#endif
