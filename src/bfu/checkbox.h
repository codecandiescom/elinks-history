/* $Id: checkbox.h,v 1.7 2003/06/27 20:44:43 zas Exp $ */

#ifndef EL__BFU_CHECKBOX_H
#define EL__BFU_CHECKBOX_H

#include "bfu/dialog.h"
#include "bfu/widget.h"
#include "terminal/terminal.h"

extern struct widget_ops checkbox_ops;
void checkboxes_width(struct terminal *term, int intl, unsigned char **texts, int *minwidth, int *maxwidth);
void dlg_format_checkbox(struct terminal *, struct terminal *, struct widget_data *, int, int *, int, int *, unsigned char *);
void dlg_format_checkboxes(struct terminal *, struct terminal *, int, struct widget_data *, int, int, int *, int, int *, unsigned char **);

void checkbox_list_fn(struct dialog_data *);

#endif
