/* $Id: group.h,v 1.15 2003/12/01 15:19:53 pasky Exp $ */

#ifndef EL__BFU_GROUP_H
#define EL__BFU_GROUP_H

struct dialog_data;
struct terminal;
struct widget_data;

void dlg_format_group(struct terminal *term,
		 struct widget_data *widget_data,
		 int n, int x, int *y, int w, int *rw);

void group_layouter(struct dialog_data *);

#endif
