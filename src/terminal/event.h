/* $Id: event.h,v 1.2 2003/09/25 19:11:33 zas Exp $ */

#ifndef EL__TERMINAL_EVENT_H
#define EL__TERMINAL_EVENT_H

enum term_event_type {
	EV_INIT,
	EV_KBD,
	EV_MOUSE,
	EV_REDRAW,
	EV_RESIZE,
	EV_ABORT,
};

/* XXX: do not change order of fields. --Zas */
struct term_event {
	enum term_event_type ev;
	long x;
	long y;
	long b;
};

struct terminal;

void term_send_event(struct terminal *, struct term_event *);
void in_term(struct terminal *);

#define INIT_TERM_EVENT(type, x, y, b) { (type), (x), (y), (b) }

#endif /* EL__TERMINAL_EVENT_H */
