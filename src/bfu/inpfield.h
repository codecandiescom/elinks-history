/* $Id: inpfield.h,v 1.34 2004/07/02 16:00:46 zas Exp $ */

#ifndef EL__BFU_INPFIELD_H
#define EL__BFU_INPFIELD_H

#include "bfu/dialog.h"
#include "bfu/style.h"
#include "bfu/widget.h"
#include "util/memlist.h"

struct input_history;
struct session;
struct terminal;

#define add_dlg_field_do(dlg, t, label, min_, max_, handler, dlen, d, hist)\
	do {								\
		struct widget *widget;					\
									\
		widget = &(dlg)->widgets[(dlg)->widgets_size++];	\
		widget->type = (t);					\
		widget->text = (label);					\
		widget->info.field.min = (min_);			\
		widget->info.field.max = (max_);			\
		widget->fn = (handler);					\
		widget->datalen = (dlen);				\
		widget->data = (d);					\
		widget->info.field.history = (hist);			\
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

enum input_line_code {
	INPUT_LINE_CANCEL,
	INPUT_LINE_PROCEED,
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
