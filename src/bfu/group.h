/* $Id: group.h,v 1.4 2003/05/04 17:25:51 pasky Exp $ */

#ifndef EL__BFU_GROUP_H
#define EL__BFU_GROUP_H

#include "bfu/dialog.h"
#include "bfu/widget.h"
#include "terminal/terminal.h"


void max_group_width(struct terminal *, unsigned char **, struct widget_data *, int, int *);
void min_group_width(struct terminal *, unsigned char **, struct widget_data *, int, int *);
void dlg_format_group(struct terminal *, struct terminal *, unsigned char **, struct widget_data *, int, int, int *, int, int *);

void group_fn(struct dialog_data *);

#endif
