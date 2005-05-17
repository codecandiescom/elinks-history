/* $Id: event.h,v 1.21 2005/05/17 12:56:58 zas Exp $ */

#ifndef EL__TERMINAL_EVENT_H
#define EL__TERMINAL_EVENT_H

struct terminal;

/* Some constants for the strings inside of {struct terminal}. */

#define MAX_TERM_LEN	32	/* this must be multiple of 8! (alignment problems) */
#define MAX_CWD_LEN	256	/* this must be multiple of 8! (alignment problems) */


enum term_event_type {
	EVENT_INIT,
	EVENT_KBD,
	EVENT_MOUSE,
	EVENT_REDRAW,
	EVENT_RESIZE,
	EVENT_ABORT,
};

/* XXX: do not change order of fields. --Zas */
struct term_event {
	enum term_event_type ev;

	union {
		/* EVENT_MOUSE */
		struct term_event_mouse {
			int x, y;
			unsigned int button;
		} mouse;

		/* EVENT_KBD */
		struct term_event_keyboard {
			int key, modifier;
		} keyboard;

		/* EVENT_INIT, EVENT_RESIZE, EVENT_REDRAW */
		struct term_event_size {
			int width, height;
		} size;
	} info;
};

static inline void
set_mouse_term_event(struct term_event *ev, int x, int y, unsigned int button)
{
	memset(ev, 0, sizeof(*ev));
	ev->ev = EVENT_MOUSE;
	ev->info.mouse.x = x;
	ev->info.mouse.y = y;
	ev->info.mouse.button = button;
}

static inline void
set_kbd_term_event(struct term_event *ev, int key, int modifier)
{
	memset(ev, 0, sizeof(*ev));
	ev->ev = EVENT_KBD;
	ev->info.keyboard.key = key;
	ev->info.keyboard.modifier = modifier;
}

static inline void
set_abort_term_event(struct term_event *ev)
{
	memset(ev, 0, sizeof(*ev));
	ev->ev = EVENT_ABORT;
}

static inline void
set_wh_term_event(struct term_event *ev, enum term_event_type type, int width, int height)
{
	memset(ev, 0, sizeof(*ev));
	ev->ev = type;
	ev->info.size.width = width;
	ev->info.size.height = height;
}

#define set_init_term_event(ev, w, h) set_wh_term_event(ev, EVENT_INIT, w, h)
#define set_resize_term_event(ev, w, h) set_wh_term_event(ev, EVENT_RESIZE, w, h)
#define set_redraw_term_event(ev, w, h) set_wh_term_event(ev, EVENT_REDRAW, w, h)


/* This holds the information used when handling the initial connection between
 * a dumb and master terminal. */
/* XXX: We might be connecting to an older ELinks or an older ELinks is
 * connecting to a newer ELinks master so for the sake of compatibility it
 * would be unwise to just change the layout of the struct. If you do have to
 * add new members add them at the bottom and use magic variables to
 * distinguish them when decoding the terminal info. */
struct terminal_info {
	struct term_event event;		/* The EVENT_INIT event */
	unsigned char name[MAX_TERM_LEN];	/* $TERM environment name */
	unsigned char cwd[MAX_CWD_LEN];		/* Current working directory */
	int system_env;				/* System info (X, screen) */
	int length;				/* Length of @data member */
	int session_info;			/* Value depends on @magic */
	int magic;				/* Identity of the connector */

	/* In the master that is connected to all bytes after @data will be
	 * interpreted as URI string information. */
	unsigned char data[1];
};

/* The @data member has to have size of one for portability but it can be
 * empty/zero so when reading and writing it we need to ignore the byte. */
#define TERMINAL_INFO_SIZE offsetof(struct terminal_info, data)

/* We use magic numbers to signal the identity of the dump client terminal.
 * Magic numbers are composed by the INTERLINK_MAGIC() macro. It is a negative
 * magic to be able to distinguish the oldest format from the newer ones. */
#define INTERLINK_MAGIC(major, minor) -(((major) << 8) + (minor))

#define INTERLINK_NORMAL_MAGIC INTERLINK_MAGIC(1, 0)
#define INTERLINK_REMOTE_MAGIC INTERLINK_MAGIC(1, 1)

void term_send_event(struct terminal *, struct term_event *);
void in_term(struct terminal *);

#define get_kbd_key(event)		((event)->info.keyboard.key)
#define check_kbd_key(event, key)	(get_kbd_key(event) == (key))

#define get_kbd_modifier(event)		((event)->info.keyboard.modifier)
#define check_kbd_modifier(event, mod)	(get_kbd_modifier(event) == (mod))

#define check_kbd_textinput_key(event)	(get_kbd_key(event) >= ' ' && get_kbd_key(event) < 256 && check_kbd_modifier(event, KBD_MOD_NONE))
#define check_kbd_label_key(event)	(get_kbd_key(event) > ' ' && get_kbd_key(event) < 256)

#endif /* EL__TERMINAL_EVENT_H */
