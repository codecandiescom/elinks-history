/* $Id: common.h,v 1.2 2004/11/19 15:40:20 zas Exp $ */

#ifndef EL__BFU_COMMON_H
#define EL__BFU_COMMON_H

/* Event handlers return this values */
typedef enum t_handler_event_status {
	EVENT_PROCESSED	= 0,
	EVENT_NOT_PROCESSED = 1
} t_handler_event_status;

/* TODO: wrapper for handler functions */
#define WIDGET_HANDLER_FUNC(fn) t_handler_event_status (*fn)(struct dialog_data *, struct widget_data *)

enum widget_type {
	WIDGET_CHECKBOX,
	WIDGET_FIELD,
	WIDGET_FIELD_PASS,
	WIDGET_BUTTON,
	WIDGET_LISTBOX,
	WIDGET_TEXT,
};

#endif /* EL__BFU_COMMON_H */
