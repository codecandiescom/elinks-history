/* $Id: event.h,v 1.11 2004/06/14 00:53:48 jonas Exp $ */

#ifndef EL__TERMINAL_EVENT_H
#define EL__TERMINAL_EVENT_H

struct terminal;

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

/* This holds the information used when handling the initial connection between
 * a dumb and master terminal. */
/* XXX: We might be connecting to an older ELinks or an older ELinks is
 * connecting to a newer ELinks master so for the sake of compatibility it
 * would be unwise to just change the layout of the struct. If you do have to
 * add new members add them at the bottom and use magic variables to
 * distinguish them when decoding the terminal info. */
struct terminal_info {
	struct term_event event;		/* The EV_INIT event */
	unsigned char name[MAX_TERM_LEN];	/* $TERM environment name */
	unsigned char cwd[MAX_CWD_LEN];		/* Current working directory */
	int system_env;				/* System info (X, screen) */
	int length;				/* Length of @data member */
	int session_info;			/* Value depends on @magic */
	int magic;				/* Identity of the connector */

	/* In the master that is connected to all bytes after @data will be
	 * interpreted as URI string information. */
	unsigned char data[0];
};

/* We use magic numbers to signal the identity of the dump client terminal.
 * Magic numbers are composed by the INTERLINK_MAGIC() macro. It is a negative
 * magic to be able to distinguish the oldest format from the newer ones. */
#define INTERLINK_MAGIC(major, minor) -(((major) << 8) + (minor))

#define INTERLINK_NORMAL_MAGIC INTERLINK_MAGIC(1, 0)
#define INTERLINK_REMOTE_MAGIC INTERLINK_MAGIC(1, 1)

void term_send_event(struct terminal *, struct term_event *);
void in_term(struct terminal *);

#define INIT_TERM_EVENT(type, x, y, b) { (type), (x), (y), (b) }

#endif /* EL__TERMINAL_EVENT_H */
