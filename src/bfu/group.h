/* $Id: group.h,v 1.9 2003/11/04 14:25:47 zas Exp $ */

#ifndef EL__BFU_GROUP_H
#define EL__BFU_GROUP_H

#include "bfu/dialog.h"
#include "bfu/widget.h"
#include "terminal/terminal.h"

#if 0	/* Unused for now. */
void max_group_width(struct terminal *term, int intl,
		struct widget_data *widget_data, int n, int *w);
void min_group_width(struct terminal *term, int intl,
		struct widget_data *widget_data, int n, int *w);
#endif
void group_width(struct terminal *term, int intl,
		 struct widget_data *widget_data, int n,
	         int *min_width, int *max_width);

void dlg_format_group(struct terminal *term, struct terminal *t2,
		 struct widget_data *widget_data,
		 int n, int x, int *y, int w, int *rw, int intl);

void group_fn(struct dialog_data *);

#endif
