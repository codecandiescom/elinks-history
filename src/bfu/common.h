/* $Id: common.h,v 1.7 2005/03/05 20:46:46 zas Exp $ */

#ifndef EL__BFU_COMMON_H
#define EL__BFU_COMMON_H

struct dialog_data;
struct widget_data;

/* Event handlers return this values */
typedef enum {
	EVENT_PROCESSED	= 0,
	EVENT_NOT_PROCESSED = 1
} widget_handler_status_T;

/* Handler type for widgets. */
typedef widget_handler_status_T (t_widget_handler)(struct dialog_data *, struct widget_data *);

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
