/* $Id: inpfield.h,v 1.27 2004/01/28 06:14:54 jonas Exp $ */

#ifndef EL__BFU_INPFIELD_H
#define EL__BFU_INPFIELD_H

#include "bfu/dialog.h"
#include "bfu/style.h"
#include "bfu/widget.h"
#include "util/memlist.h"

struct input_history;
struct session;
struct terminal;

#define add_dlg_field_do(dlg, t, label, min_, max_, handler, datalen_, data_, hist)	\
	do {								\
		int n = (dlg)->widgets_size;				\
		(dlg)->widgets[n].type = (t);				\
		(dlg)->widgets[n].text = (label);			\
		(dlg)->widgets[n].info.field.min = (min_);		\
		(dlg)->widgets[n].info.field.max = (max_);		\
		(dlg)->widgets[n].fn = (handler);			\
		(dlg)->widgets[n].datalen = (datalen_);			\
		(dlg)->widgets[n].data = (data_);			\
		(dlg)->widgets[n].info.field.history = (hist);		\
		(dlg)->widgets_size++;					\
	} while (0)

#define add_dlg_field(dlg, label, min, max, handler, len, field, history)	\
	add_dlg_field_do(dlg, WIDGET_FIELD, label, min, max, handler, len, field, history)

#define add_dlg_field_pass(dlg, label, min, max, handler, len, field)	\
	add_dlg_field_do(dlg, WIDGET_FIELD_PASS, label, min, max, handler, len, field, NULL)

extern struct widget_ops field_ops;
extern struct widget_ops field_pass_ops;

int check_number(struct dialog_data *, struct widget_data *);
int check_nonempty(struct dialog_data *, struct widget_data *);

void dlg_format_field(struct terminal *, struct widget_data *, int, int *, int, int *, enum format_align);

void input_field(struct terminal *, struct memory_list *, int, unsigned char *,
		 unsigned char *, unsigned char *, unsigned char *, void *,
		 struct input_history *, int, unsigned char *, int, int,
		 int (*)(struct dialog_data *, struct widget_data *),
		 void (*)(void *, unsigned char *),
		 void (*)(void *));


/* Input lines */

#define INPUT_LINE_BUFFER_SIZE	80
#define INPUT_LINE_WIDGETS	1

/* If the handler returns non zero value it means to cancel the input line */
typedef int (*input_line_handler)(struct session *ses, int action, unsigned char *buffer);

void
input_field_line(struct session *ses, unsigned char *prompt,
		 struct input_history *history, input_line_handler handler);

#endif
