/* $Id: checkbox.h,v 1.5 2003/06/07 16:21:07 pasky Exp $ */

#ifndef EL__BFU_CHECKBOX_H
#define EL__BFU_CHECKBOX_H

#include "bfu/dialog.h"
#include "bfu/widget.h"
#include "terminal/terminal.h"

extern struct widget_ops checkbox_ops;

void checkboxes_width(struct terminal *, int, unsigned char **, int *, void (*)(struct terminal *, unsigned char *, int *));
void dlg_format_checkbox(struct terminal *, struct terminal *, struct widget_data *, int, int *, int, int *, unsigned char *);
void dlg_format_checkboxes(struct terminal *, struct terminal *, int, struct widget_data *, int, int, int *, int, int *, unsigned char **);

void checkbox_list_fn(struct dialog_data *);

#endif
