/* $Id: inpfield.h,v 1.17 2003/10/28 19:03:56 jonas Exp $ */

#ifndef EL__BFU_INPFIELD_H
#define EL__BFU_INPFIELD_H

#include "bfu/dialog.h"
#include "bfu/inphist.h"
#include "bfu/style.h"
#include "bfu/widget.h"
#include "terminal/terminal.h"
#include "util/memlist.h"

#define add_dlg_field_do(dlg, n, t, min_, max_, handler, datalen_, data_, hist)	\
	do {								\
		(dlg)->widgets[n].type = (t);				\
		(dlg)->widgets[n].info.field.min = (min_);		\
		(dlg)->widgets[n].info.field.max = (max_);		\
		(dlg)->widgets[n].fn = (handler);			\
		(dlg)->widgets[n].datalen = (datalen_);			\
		(dlg)->widgets[n].data = (data_);			\
		(dlg)->widgets[n].info.field.history = (hist);		\
		(n)++;							\
	} while (0)

#define add_dlg_field(dlg, n, min, max, handler, len, field, history)	\
	add_dlg_field_do(dlg, n, WIDGET_FIELD, min, max, handler, len, field, history)

#define add_dlg_field_pass(dlg, n, min, max, handler, len, field)	\
	add_dlg_field_do(dlg, n, WIDGET_FIELD_PASS, min, max, handler, len, field, NULL)

extern struct widget_ops field_ops;
extern struct widget_ops field_pass_ops;

int check_number(struct dialog_data *, struct widget_data *);
int check_nonempty(struct dialog_data *, struct widget_data *);

void dlg_format_field(struct terminal *, struct terminal *, struct widget_data *, int, int *, int, int *, enum format_align);

void input_field_fn(struct dialog_data *);
void input_field(struct terminal *, struct memory_list *, int, unsigned char *,
		 unsigned char *, unsigned char *, unsigned char *, void *,
		 struct input_history *, int, unsigned char *, int, int,
		 int (*)(struct dialog_data *, struct widget_data *),
		 void (*)(void *, unsigned char *),
		 void (*)(void *));

#endif
