/* $Id: checkbox.h,v 1.3 2002/07/05 00:29:57 pasky Exp $ */

#ifndef EL__BFU_CHECKBOX_H
#define EL__BFU_CHECKBOX_H

#include "bfu/dialog.h"
#include "bfu/widget.h"
#include "lowlevel/terminal.h"

extern struct widget_ops checkbox_ops;

void checkboxes_width(struct terminal *, unsigned char **, int *, void (*)(struct terminal *, unsigned char *, int *));
void dlg_format_checkbox(struct terminal *, struct terminal *, struct widget_data *, int, int *, int, int *, unsigned char *);
void dlg_format_checkboxes(struct terminal *, struct terminal *, struct widget_data *, int, int, int *, int, int *, unsigned char **);

void checkbox_list_fn(struct dialog_data *);

#endif
