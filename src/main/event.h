/* $Id: event.h,v 1.5 2003/09/23 14:03:06 pasky Exp $ */

#ifndef EL__SCHED_EVENT_H
#define EL__SCHED_EVENT_H

#include <stdarg.h>

#define EVENT_NONE (-1)


/* This enum is returned by each event hook and determines whether we should
 * go on in the chain or finish the event processing. You want to always
 * return EHS_NEXT. */
enum evhook_status {
	EHS_NEXT,
	EHS_LAST,
};


/*** The life of events */

/* This registers an event of name @name, allocating an id number for it. */
/* The function returns the id or negative number upon error. */
int register_event(unsigned char *name);

/* This unregisters an event number @event, freeing the resources it
 * occupied, chain of associated hooks and unallocating the event id for
 * further recyclation. */
int unregister_event(int event);

int register_event_hook(int id, enum evhook_status (*callback)(va_list ap),
			int priority);

void unregister_event_hook(int id, enum evhook_status (*callback)(va_list ap));


/*** The events resolver */

/* This looks up the events table and returns the event id associated
 * with a given event @name. The event id is guaranteed not to change
 * during the event lifetime (that is, between its registration and
 * unregistration), thus it may be cached intermediatelly. */
/* It returns the event id on success or a negative number upon failure
 * (ie. there is no such event). */
int get_event_id(unsigned char *name);

/* This looks up the events table and returns the name of a given event
 * @id. */
/* It returns the event name on success (statically allocated, you are
 * not permitted to modify it) or NULL upon failure (ie. there is no
 * such event). */
unsigned char *get_event_name(int id);

#define set_event_id(event, name) 			\
	do { 						\
		if (event == EVENT_NONE) 		\
			event = get_event_id(name);	\
	} while (0)


/*** The events generator */

void trigger_event(int id, ...);


/*** The very events subsystem itself */

void init_event(void);

void done_event(void);


#endif
