/* Event system support routines. */
/* $Id: event.c,v 1.65 2004/07/01 19:27:02 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "elinks.h"

#include "intl/gettext/libintl.h"
#include "lowlevel/timer.h"
#include "main.h"			/* terminate */
#include "sched/session.h"
#include "terminal/draw.h"
#include "terminal/event.h"
#include "terminal/kbd.h"
#include "terminal/tab.h"
#include "terminal/terminal.h"
#include "terminal/screen.h"
#include "terminal/window.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/object.h"
#include "util/string.h"


/* Information used for communication between ELinks instances */
struct terminal_interlink {
	/* How big the input queue is and how much is free */
	int qlen;
	int qfreespace;

	/* Something weird regarding the UTF8 I/O. */
	struct {
		unicode_val ucs;
		int len;
		int min;
	} utf_8;

	/* This is the queue of events as coming from the other ELinks instance
	 * owning the hosting terminal. */
	unsigned char input_queue[1];
};


void
term_send_event(struct terminal *term, struct term_event *ev)
{
	struct window *win;

	assert(ev && term);
	if_assert_failed return;

	switch (ev->ev) {
	case EV_INIT:
	case EV_RESIZE:
		if (ev->x < 0 || ev->y < 0) {
			ERROR(_("Bad terminal size: %d, %d", term),
			      (int) ev->x, (int) ev->y);
			break;
		}

		resize_screen(term, ev->x, ev->y);
		erase_screen(term);
		/* Fall through */

	case EV_REDRAW:
		/* Nasty hack to avoid assertion failures when doing -remote
		 * stuff and the client exits right away */
		if (!term->screen->image) break;

		clear_terminal(term);
		term->redrawing = 2;
		/* Note that you do NOT want to ever go and create new
		 * window inside EV_INIT handler (it'll get second
		 * EV_INIT here). Perhaps the best thing you could do
		 * is registering a bottom-half handler which will open
		 * additional windows.
		 * --pasky */
		if (ev->ev == EV_RESIZE) {
			/* We want to propagate EV_RESIZE even to inactive
			 * tabs! Nothing wrong will get drawn (in the final
			 * result) as the active tab is always the first one,
			 * thus will be drawn last here. Thanks, Witek!
			 * --pasky */
			foreachback (win, term->windows)
				win->handler(win, ev, 0);

		} else {
			foreachback (win, term->windows)
				if (!inactive_tab(win))
					win->handler(win, ev, 0);
		}
		term->redrawing = 0;
		break;

	case EV_MOUSE:
	case EV_KBD:
	case EV_ABORT:
		assert(!list_empty(term->windows));
		if_assert_failed break;

		/* We need to send event to correct tab, not to the first one. --karpov */
		/* ...if we want to send it to a tab at all. --pasky */
		win = term->windows.next;
		if (win->type == WT_TAB) {
			win = get_current_tab(term);
			assertm(win, "No tab to send the event to!");
			if_assert_failed return;
		}

		win->handler(win, ev, 0);
	}
}

static void
term_send_ucs(struct terminal *term, struct term_event *ev, unicode_val u)
{
	unsigned char *recoded;

	recoded = u2cp_no_nbsp(u, get_opt_int_tree(term->spec, "charset"));
	if (!recoded) recoded = "*";
	while (*recoded) {
		ev->x = *recoded;
		term_send_event(term, ev);
		recoded++;
	}
}

static void
check_terminal_name(struct terminal *term, struct terminal_info *info)
{
	unsigned char name[MAX_TERM_LEN + 10];
	int i;

	/* We check TERM env. var for sanity, and fallback to _template_ if
	 * needed. This way we prevent elinks.conf potential corruption. */
	for (i = 0; info->name[i]; i++) {
		if (!isA(info->name[i])) {
			usrerror(_("Warning: terminal name contains illicit chars.", term));
			return;
		}
	}

	strcpy(name, "terminal.");
	strcat(name, info->name);

	/* Unlock the default _template_ option tree that was asigned by
	 * init_term() and get the correct one. */
	object_unlock(term->spec);
	term->spec = get_opt_rec(config_options, name);
	object_lock(term->spec);
}

static int
handle_interlink_event(struct terminal *term, struct term_event *ev)
{
	struct terminal_info *info = NULL;
	struct terminal_interlink *interlink = term->interlink;

	switch (ev->ev) {
	case EV_INIT:
		if (interlink->qlen < sizeof(struct terminal_info))
			return 0;

		info = (struct terminal_info *) ev;

		if (interlink->qlen < sizeof(struct terminal_info) + info->length)
			return 0;

		info->name[MAX_TERM_LEN - 1] = 0;
		check_terminal_name(term, info);

		memcpy(term->cwd, info->cwd, MAX_CWD_LEN);
		term->cwd[MAX_CWD_LEN - 1] = 0;

		term->environment = info->system_env;

		/* We need to make sure that it is possible to draw on the
		 * terminal screen before decoding the session info so that
		 * handling of bad URL syntax by openning msg_box() will be
		 * possible. */
		term_send_event(term, ev);

		/* Either the initialization of the first session failed or we
		 * are doing a remote session so quit.*/
		if (!decode_session_info(term, info)) {
			destroy_terminal(term);
			return 0;
		}

		ev->ev = EV_REDRAW;
		/* Fall through */
	case EV_REDRAW:
	case EV_RESIZE:
		term_send_event(term, ev);
		break;

	case EV_MOUSE:
#ifdef CONFIG_MOUSE
		reset_timer();
		term_send_event(term, ev);
#endif
		break;

	case EV_KBD:
	{
		int utf8_io = -1;

		reset_timer();

		if (ev->y == KBD_CTRL && toupper(ev->x) == 'L') {
			redraw_terminal_cls(term);
			break;

		} else if (ev->x == KBD_CTRL_C) {
			destroy_terminal(term);
			return 0;
		}

		if (interlink->utf_8.len) {
			utf8_io = get_opt_bool_tree(term->spec, "utf_8_io");

			if ((ev->x & 0xC0) == 0x80
			    && utf8_io) {
				interlink->utf_8.ucs <<= 6;
				interlink->utf_8.ucs |= ev->x & 0x3F;
				if (! --interlink->utf_8.len) {
					unicode_val u = interlink->utf_8.ucs;

					if (u < interlink->utf_8.min)
						u = UCS_NO_CHAR;
					term_send_ucs(term, ev, u);
				}
				break;

			} else {
				interlink->utf_8.len = 0;
				term_send_ucs(term, ev, UCS_NO_CHAR);
			}
		}

		if (ev->x < 0x80 || ev->x > 0xFF
		    || (utf8_io == -1
			? !get_opt_bool_tree(term->spec, "utf_8_io")
			: !utf8_io)) {

			term_send_event(term, ev);
			break;

		} else if ((ev->x & 0xC0) == 0xC0
			   && (ev->x & 0xFE) != 0xFE) {
			unsigned int mask, cov = 0x80;
			int len = 0;

			for (mask = 0x80; ev->x & mask; mask >>= 1) {
				len++;
				interlink->utf_8.min = cov;
				cov = 1 << (1 + 5 * len);
			}
			interlink->utf_8.len = len - 1;
			interlink->utf_8.ucs = ev->x & (mask - 1);
			break;
		}

		term_send_ucs(term, ev, UCS_NO_CHAR);
		break;
	}

	case EV_ABORT:
		destroy_terminal(term);
		return 0;

	default:
		ERROR(_("Bad event %d", term), ev->ev);
	}

	/* For EV_INIT we read a liitle more */
	if (info) return sizeof(struct terminal_info) + info->length;
	return sizeof(struct term_event);
}

void
in_term(struct terminal *term)
{
	struct terminal_interlink *interlink = term->interlink;
	int r;
	unsigned char *iq;

	if (!interlink
	    || !interlink->qfreespace
	    || interlink->qfreespace - interlink->qlen > ALLOC_GR) {
		int qlen = interlink ? interlink->qlen : 0;
		int queuesize = ((qlen + ALLOC_GR) & ~(ALLOC_GR - 1));
		int newsize = sizeof(struct terminal_interlink) + queuesize;

		interlink = mem_realloc(interlink, newsize);
		if (!interlink) {
			destroy_terminal(term);
			return;
		}

		/* Blank the members for the first allocation */
		if (!term->interlink)
			memset(interlink, 0, sizeof(struct terminal_interlink));

		term->interlink = interlink;
		interlink->qfreespace = queuesize - interlink->qlen;
	}

	iq = interlink->input_queue;
	r = safe_read(term->fdin, iq + interlink->qlen, interlink->qfreespace);
	if (r <= 0) {
		if (r == -1 && errno != ECONNRESET)
			ERROR(_("Could not read event: %d (%s)", term),
			      errno, (unsigned char *) strerror(errno));

		destroy_terminal(term);
		return;
	}

	interlink->qlen += r;
	interlink->qfreespace -= r;

	while (interlink->qlen >= sizeof(struct term_event)) {
		struct term_event *ev = (struct term_event *) iq;
		int event_size = handle_interlink_event(term, ev);

		/* If the event was not handled save the bytes in the queue for
		 * later in case more stuff is read later. */
		if (!event_size) break;

		/* Acount for the handled bytes */
		interlink->qlen -= event_size;
		interlink->qfreespace += event_size;

		/* If there are no more bytes to handle stop else move next
		 * event bytes to the front of the queue. */
		if (!interlink->qlen) break;
		memmove(iq, iq + event_size, interlink->qlen);
	}
}
