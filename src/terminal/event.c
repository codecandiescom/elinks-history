/* Event system support routines. */
/* $Id: event.c,v 1.4 2003/07/26 10:39:47 pasky Exp $ */

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
#include "sched/session.h"
#include "terminal/event.h"
#include "terminal/kbd.h"
#include "terminal/tab.h"
#include "terminal/terminal.h"
#include "terminal/screen.h"
#include "terminal/window.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"


void
term_send_event(struct terminal *term, struct event *ev)
{
	struct window *win;

	assert(ev && term && term->windows.next);
	if_assert_failed return;

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

static void
term_send_ucs(struct terminal *term, struct event *ev, unicode_val u)
{
	unsigned char *recoded;

	if (u == 0xA0) u = ' ';
	recoded = u2cp(u, get_opt_int_tree(term->spec, "charset"));
	if (!recoded) recoded = "*";
	while (*recoded) {
		ev->x = *recoded;
		term_send_event(term, ev);
		recoded++;
	}
}


void
in_term(struct terminal *term)
{
	struct event *ev;
	int r;
	unsigned char *iq = term->input_queue;

	if (!iq || !term->qfreespace || term->qfreespace - term->qlen > ALLOC_GR) {
		int newsize = ((term->qlen + ALLOC_GR) & ~(ALLOC_GR - 1));

		iq = mem_realloc(term->input_queue, newsize);
		if (!iq) {
			destroy_terminal(term);
			return;
		}
		term->input_queue = iq;
		term->qfreespace = newsize - term->qlen;
	}

	r = read(term->fdin, iq + term->qlen, term->qfreespace);
	if (r <= 0) {
		if (r == -1 && errno != ECONNRESET)
			error(_("Could not read event: %d (%s)", term),
			      errno, (unsigned char *) strerror(errno));
		destroy_terminal(term);
		return;
	}
	term->qlen += r;
	term->qfreespace -= r;

test_queue:
	if (term->qlen < sizeof(struct event)) return;
	ev = (struct event *)iq;
	r = sizeof(struct event);

	if (ev->ev != EV_INIT
	    && ev->ev != EV_RESIZE
	    && ev->ev != EV_REDRAW
	    && ev->ev != EV_KBD
	    && ev->ev != EV_MOUSE
	    && ev->ev != EV_ABORT) {
		error(_("Bad event %d", term), ev->ev);
		goto mm;
	}

	if (ev->ev == EV_INIT) {
		int init_len;
		int evterm_len = sizeof(struct event) + MAX_TERM_LEN;
		int evtermcwd_len = evterm_len + MAX_CWD_LEN;
		int evtermcwd1int_len = evtermcwd_len + sizeof(int);
		int evtermcwd2int_len = evtermcwd1int_len + sizeof(int);

		if (term->qlen < evtermcwd2int_len) return;
		init_len = *(int *)(iq + evtermcwd1int_len);

		if (term->qlen < evtermcwd2int_len + init_len) return;

		memcpy(term->term, iq + sizeof(struct event), MAX_TERM_LEN);
		term->term[MAX_TERM_LEN - 1] = 0;

		{
			unsigned char name[MAX_TERM_LEN + 10];
			int i = 0, badchar = 0;

			strcpy(name, "terminal.");

			/* We check TERM env. var for sanity, and fallback to
			 * _template_ if needed. This way we prevent
			 * elinks.conf potential corruption. */
			while (term->term[i]) {
				if (!isA(term->term[i])) {
					badchar = 1;
					break;
				}
				i++;
			}

			if (badchar) {
				error(_("Warning: terminal name contains illicit chars.", term));
				strcat(name, "_template_");
			} else {
				strcat(name, term->term);
			}

			term->spec = get_opt_rec(config_options, name);
		}

		memcpy(term->cwd, iq + evterm_len, MAX_CWD_LEN);
		term->cwd[MAX_CWD_LEN - 1] = 0;

		term->environment = *(int *)(iq + evtermcwd_len);
		ev->b = (long) decode_session_info(iq + evtermcwd1int_len);
		r = evtermcwd2int_len + init_len;
	}

	if (ev->ev == EV_REDRAW || ev->ev == EV_RESIZE || ev->ev == EV_INIT) {
		struct window *win;

send_redraw:
		if (ev->x < 0 || ev->y < 0) {
			error(_("Bad terminal size: %d, %d", term),
			      (int) ev->x, (int) ev->y);
			goto mm;
		}

		alloc_screen(term, ev->x, ev->y);
		clear_terminal(term);
		erase_screen(term);
		term->redrawing = 1;
		foreachback (win, term->windows) {
			/* Note that you do NOT want to ever go and create new
			 * window inside EV_INIT handler (it'll get second
			 * EV_INIT here). Perhaps the best thing you could do
			 * is registering a bottom-half handler which will open
			 * additional windows.
			 * --pasky */
			/* We want to propagate EV_RESIZE even to inactive
			 * tabs! Nothing wrong will get drawn (in the final
			 * result) as the active tab is always the first one,
			 * thus will be drawn last here. Thanks, Witek!
			 * --pasky */
			if (!inactive_tab(win) || ev->ev == EV_RESIZE)
				win->handler(win, ev, 0);
		}
		term->redrawing = 0;
	}

	if (ev->ev == EV_KBD || ev->ev == EV_MOUSE) {
		reset_timer();
		if (ev->ev == EV_KBD && upcase(ev->x) == 'L'
		    && ev->y == KBD_CTRL) {
			ev->ev = EV_REDRAW;
			ev->x = term->x;
			ev->y = term->y;
			goto send_redraw;
		}
		else if (ev->ev == EV_KBD && ev->x == KBD_CTRL_C)
			((struct window *) &term->windows)->prev->handler(term->windows.prev, ev, 0);
		else if (ev->ev == EV_KBD) {
			if (term->utf_8.len) {
				if ((ev->x & 0xC0) == 0x80
				    && get_opt_bool_tree(term->spec, "utf_8_io")) {
					term->utf_8.ucs <<= 6;
					term->utf_8.ucs |= ev->x & 0x3F;
					if (! --term->utf_8.len) {
						unicode_val u = term->utf_8.ucs;

						if (u < term->utf_8.min) u = UCS_NO_CHAR;
						term_send_ucs(term, ev, u);
					}
					goto mm;
				} else {
					term->utf_8.len = 0;
					term_send_ucs(term, ev, UCS_NO_CHAR);
				}
			}
			if (ev->x < 0x80 || ev->x > 0xFF
			    || !get_opt_bool_tree(term->spec, "utf_8_io")) {
				term_send_event(term, ev);
				goto mm;
			} else if ((ev->x & 0xC0) == 0xC0 && (ev->x & 0xFE) != 0xFE) {
				int mask, len = 0, cov = 0x80;

				for (mask = 0x80; ev->x & mask; mask >>= 1) {
					len++;
					term->utf_8.min = cov;
					cov = 1 << (1 + 5 * len);
				}
				term->utf_8.len = len - 1;
				term->utf_8.ucs = ev->x & (mask - 1);
				goto mm;
			}
			term_send_ucs(term, ev, UCS_NO_CHAR);
		} else term_send_event(term, ev);
	}

	if (ev->ev == EV_ABORT) destroy_terminal(term);
	/* redraw_screen(term); */
mm:
	if (term->qlen == r) {
		term->qlen = 0;
	} else {
		term->qlen -= r;
		memmove(iq, iq + r, term->qlen);
	}
	term->qfreespace += r;

	goto test_queue;
}
