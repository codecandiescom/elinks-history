/* $Id: group.h,v 1.6 2003/10/05 20:47:34 pasky Exp $ */

#ifndef EL__BFU_GROUP_H
#define EL__BFU_GROUP_H

#include "bfu/dialog.h"
#include "bfu/widget.h"
#include "terminal/terminal.h"

void max_group_width(struct terminal *term, int intl, unsigned char **texts,
		struct widget_data *item, int n, int *w);
void min_group_width(struct terminal *term, int intl, unsigned char **texts,
		struct widget_data *item, int n, int *w);
void dlg_format_group(struct terminal *term, struct terminal *t2, int intl,
		 unsigned char **texts, struct widget_data *item,
		 int n, int x, int *y, int w, int *rw);

void group_fn(struct dialog_data *);

#endif
