/* $Id: inpfield.h,v 1.8 2003/10/24 23:23:46 pasky Exp $ */

#ifndef EL__BFU_INPFIELD_H
#define EL__BFU_INPFIELD_H

#include "bfu/dialog.h"
#include "bfu/inphist.h"
#include "bfu/style.h"
#include "bfu/widget.h"
#include "terminal/terminal.h"
#include "util/memlist.h"

#define set_dlg_field_do(dlg, n, t, min, max, handler, len, field, history)	\
	do {								\
		(dlg)->items[n].type = (t);				\
		(dlg)->items[n].gid = (min);				\
		(dlg)->items[n].gnum = (max);				\
		(dlg)->items[n].fn = (handler);				\
		(dlg)->items[n].dlen = (length);			\
		(dlg)->items[n].data = (field);				\
		(dlg)->items[n].history = (history);			\
		(n)++;						\
	} while (0)

#define set_dlg_field(dlg, n, min, max, handler, len, field, history)	\
	set_dlg_field_do(dlg, n, D_FIELD, min, max, handler, len, field, history)

#define set_dlg_field_pass(dlg, n, min, max, handler, len, field, history)	\
	set_dlg_field_do(dlg, n, D_FIELD_PASS, min, max, handler, len, field, history)

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
