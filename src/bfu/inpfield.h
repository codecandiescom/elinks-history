/* $Id: inpfield.h,v 1.39 2004/11/19 10:04:45 zas Exp $ */

#ifndef EL__BFU_INPFIELD_H
#define EL__BFU_INPFIELD_H

#include "bfu/dialog.h"
#include "bfu/style.h"
#include "bfu/widget.h"
#include "util/memlist.h"

struct input_history;
struct session;
struct terminal;

#define add_dlg_field_do(dlg, t, label, min_, max_, handler, dlen, d, hist, float_)\
	do {								\
		struct widget *widget;					\
									\
		widget = &(dlg)->widgets[(dlg)->number_of_widgets++];	\
		widget->type = (t);					\
		widget->text = (label);					\
		widget->info.field.min = (min_);			\
		widget->info.field.max = (max_);			\
		widget->fn = (handler);					\
		widget->datalen = (dlen);				\
		widget->data = (d);					\
		widget->info.field.history = (hist);			\
		widget->info.field.float_label = (float_);		\
	} while (0)

#define add_dlg_field(dlg, label, min, max, handler, len, field, history)	\
	add_dlg_field_do(dlg, WIDGET_FIELD, label, min, max, handler, len, field, history, 0)

#define add_dlg_field_float(dlg, label, min, max, handler, len, field, history)	\
	add_dlg_field_do(dlg, WIDGET_FIELD, label, min, max, handler, len, field, history, 1)

#define add_dlg_field_pass(dlg, label, min, max, handler, len, field)	\
	add_dlg_field_do(dlg, WIDGET_FIELD_PASS, label, min, max, handler, len, field, NULL, 0)

#define add_dlg_field_float_pass(dlg, label, min, max, handler, len, field)	\
	add_dlg_field_do(dlg, WIDGET_FIELD_PASS, label, min, max, handler, len, field, NULL, 1)


extern struct widget_ops field_ops;
extern struct widget_ops field_pass_ops;

t_handler_event_status check_number(struct dialog_data *, struct widget_data *);
t_handler_event_status check_nonempty(struct dialog_data *, struct widget_data *);

void dlg_format_field(struct terminal *, struct widget_data *, int, int *, int, int *, enum format_align);

void input_field(struct terminal *, struct memory_list *, int, unsigned char *,
		 unsigned char *, unsigned char *, unsigned char *, void *,
		 struct input_history *, int, unsigned char *, int, int,
		 t_handler_event_status (*)(struct dialog_data *, struct widget_data *),
		 void (*)(void *, unsigned char *),
		 void (*)(void *));


/* Input lines */

#define INPUT_LINE_BUFFER_SIZE	256
#define INPUT_LINE_WIDGETS	1

enum input_line_code {
	INPUT_LINE_CANCEL,
	INPUT_LINE_PROCEED,
	INPUT_LINE_REWIND,
};

struct input_line;

/* If the handler returns non zero value it means to cancel the input line */
typedef enum input_line_code (*input_line_handler)(struct input_line *line, int action);

struct input_line {
	struct session *ses;
	input_line_handler handler;
	void *data;
	unsigned char buffer[INPUT_LINE_BUFFER_SIZE];
};

void
input_field_line(struct session *ses, unsigned char *prompt, void *data,
		 struct input_history *history, input_line_handler handler);

#endif
