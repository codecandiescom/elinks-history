/* $Id: inpfield.h,v 1.3 2002/07/05 00:29:57 pasky Exp $ */

#ifndef EL__BFU_INPFIELD_H
#define EL__BFU_INPFIELD_H

#include "bfu/align.h"
#include "bfu/dialog.h"
#include "bfu/inphist.h"
#include "bfu/widget.h"
#include "lowlevel/terminal.h"
#include "util/memlist.h"

extern struct widget_ops field_ops;
extern struct widget_ops field_pass_ops;

int check_number(struct dialog_data *, struct widget_data *);
int check_nonempty(struct dialog_data *, struct widget_data *);

void dlg_format_field(struct terminal *, struct terminal *, struct widget_data *, int, int *, int, int *, enum format_align);

void input_field_fn(struct dialog_data *);
void input_field(struct terminal *, struct memory_list *, unsigned char *,
		 unsigned char *, unsigned char *, unsigned char *, void *,
		 struct input_history *, int, unsigned char *, int, int,
		 int (*)(struct dialog_data *, struct widget_data *),
		 void (*)(void *, unsigned char *),
		 void (*)(void *));

#endif
