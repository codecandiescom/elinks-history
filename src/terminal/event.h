/* $Id: event.h,v 1.5 2004/05/24 18:05:42 jonas Exp $ */

#ifndef EL__TERMINAL_EVENT_H
#define EL__TERMINAL_EVENT_H

/* Some constants for the strings inside of {struct terminal}. */

#define MAX_TERM_LEN	32	/* this must be multiple of 8! (alignment problems) */
#define MAX_CWD_LEN	256	/* this must be multiple of 8! (alignment problems) */


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

struct terminal_info {
	struct term_event event;
	unsigned char term[MAX_TERM_LEN];
	unsigned char cwd[MAX_CWD_LEN];
	int system_env;
	int length;
	unsigned char data[0];
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
