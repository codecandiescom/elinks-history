/* $Id: event.h,v 1.4 2004/01/30 19:13:29 jonas Exp $ */

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

#define get_mouse_action(event)		 ((event)->b & BM_ACT)
#define check_mouse_action(event, value) (get_mouse_action(event) == (value))

#define get_mouse_button(event)		 ((event)->b & BM_BUTT)
#define check_mouse_button(event, value) (get_mouse_button(event) == (value))
#define check_mouse_wheel(event)	 (get_mouse_button(event) >= B_WHEEL_UP)

#endif /* EL__TERMINAL_EVENT_H */
