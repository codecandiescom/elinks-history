/* $Id: event.h,v 1.1 2003/07/25 13:21:11 pasky Exp $ */

#ifndef EL__TERMINAL_EVENT_H
#define EL__TERMINAL_EVENT_H

enum event_type {
	EV_INIT,
	EV_KBD,
	EV_MOUSE,
	EV_REDRAW,
	EV_RESIZE,
	EV_ABORT,
};

/* XXX: do not change order of fields. --Zas */
struct event {
	enum event_type ev;
	long x;
	long y;
	long b;
};

struct terminal;

void term_send_event(struct terminal *, struct event *);

void in_term(struct terminal *);

#endif /* EL__TERMINAL_EVENT_H */
