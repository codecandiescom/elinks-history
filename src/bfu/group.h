/* $Id: group.h,v 1.11 2003/11/04 23:25:42 jonas Exp $ */

#ifndef EL__BFU_GROUP_H
#define EL__BFU_GROUP_H

#include "bfu/dialog.h"
#include "bfu/widget.h"
#include "terminal/terminal.h"

void group_width(struct terminal *term,
		 struct widget_data *widget_data, int n,
	         int *min_width, int *max_width);

void dlg_format_group(struct terminal *term, struct terminal *t2,
		 struct widget_data *widget_data,
		 int n, int x, int *y, int w, int *rw);

void group_fn(struct dialog_data *);

#endif
