/* $Id: common.h,v 1.1 2004/11/19 15:33:07 zas Exp $ */

#ifndef EL__BFU_COMMON_H
#define EL__BFU_COMMON_H

/* Event handlers return this values */
typedef enum t_handler_event_status {
	EVENT_PROCESSED	= 0,
	EVENT_NOT_PROCESSED = 1
} t_handler_event_status;

/* TODO: wrapper for handler functions */
#define WIDGET_HANDLER_FUNC(fn) t_handler_event_status (*fn)(struct dialog_data *, struct widget_data *)


#endif /* EL__BFU_COMMON_H */
