/* $Id: group.h,v 1.1 2002/07/04 15:45:38 pasky Exp $ */

#ifndef EL__BFU_GROUP_H
#define EL__BFU_GROUP_H

#include "bfu/bfu.h"
#include "lowlevel/terminal.h"


void max_group_width(struct terminal *, unsigned char **, struct widget_data *, int, int *);
void min_group_width(struct terminal *, unsigned char **, struct widget_data *, int, int *);
void dlg_format_group(struct terminal *, struct terminal *, unsigned char **, struct widget_data *, int, int, int *, int, int *);

void group_fn(struct dialog_data *);

#endif
