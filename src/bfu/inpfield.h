/* $Id: inpfield.h,v 1.43 2004/11/19 17:07:18 zas Exp $ */

#ifndef EL__BFU_INPFIELD_H
#define EL__BFU_INPFIELD_H

#include "bfu/common.h"
#include "bfu/style.h"
#include "util/memlist.h"
#include "util/lists.h"

struct dialog;
struct dialog_data;
struct input_history;
struct session;
struct terminal;
struct widget_data;

struct widget_info_field {
	int min;
	int max;
	struct input_history *history;
	int float_label;
};

struct widget_data_info_field {
	int vpos;
	int cpos;
	struct list_head history;
	struct input_history_entry *cur_hist;
};

void
add_dlg_field_do(struct dialog *dlg, enum widget_type type, unsigned char *label,
		 int min, int max, WIDGET_HANDLER_FUNC(handler),
		 int data_len, void *data,
		 struct input_history *history, int float_);

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
