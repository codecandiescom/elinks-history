/* $Id: common.h,v 1.6 2005/03/05 20:40:01 zas Exp $ */

#ifndef EL__BFU_COMMON_H
#define EL__BFU_COMMON_H

struct dialog_data;
struct widget_data;

/* Event handlers return this values */
typedef enum {
	EVENT_PROCESSED	= 0,
	EVENT_NOT_PROCESSED = 1
} t_handler_event_status;

/* Handler type for widgets. */
typedef t_handler_event_status (t_widget_handler)(struct dialog_data *, struct widget_data *);

/* Type of widgets. */
enum widget_type {
	WIDGET_CHECKBOX,
	WIDGET_FIELD,
	WIDGET_FIELD_PASS,
	WIDGET_BUTTON,
	WIDGET_LISTBOX,
	WIDGET_TEXT,
};


#endif /* EL__BFU_COMMON_H */
