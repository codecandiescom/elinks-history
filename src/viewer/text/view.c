/* HTML viewer (and much more) */
/* $Id: view.c,v 1.537 2004/06/26 22:57:45 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "elinks.h"

#include "bfu/inpfield.h"
#include "bfu/menu.h"
#include "bfu/msgbox.h"
#include "config/kbdbind.h"
#include "config/options.h"
#include "dialogs/document.h"
#include "dialogs/menu.h"
#include "dialogs/options.h"
#include "dialogs/status.h"
#include "document/document.h"
#include "document/html/frames.h"
#include "document/options.h"
#include "document/renderer.h"
#include "intl/charsets.h"
#include "intl/gettext/libintl.h"
#include "osdep/osdep.h"
#include "protocol/uri.h"
#include "sched/action.h"
#include "sched/download.h"
#include "sched/event.h"
#include "sched/location.h"
#include "sched/session.h"
#include "sched/task.h"
#include "terminal/draw.h"
#include "terminal/kbd.h"
#include "terminal/mouse.h"
#include "terminal/tab.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/color.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/snprintf.h"
#include "util/string.h"
#include "viewer/dump/dump.h"
#include "viewer/text/draw.h"
#include "viewer/text/form.h"
#include "viewer/text/link.h"
#include "viewer/text/marks.h"
#include "viewer/text/search.h"
#include "viewer/text/textarea.h"
#include "viewer/text/view.h"
#include "viewer/text/vs.h"


/* FIXME: Add comments!! --Zas */
/* TODO: This file needs to be splitted to many smaller ones. Definitively.
 * --pasky */

void
detach_formatted(struct document_view *doc_view)
{
	assert(doc_view);
	if_assert_failed return;

	if (doc_view->document) {
		release_document(doc_view->document);
		doc_view->document = NULL;
	}
	doc_view->vs = NULL;
	if (doc_view->link_bg) free_link(doc_view);
	mem_free_set(&doc_view->name, NULL);
}

/* type == 0 -> PAGE_DOWN
 * type == 1 -> DOWN */
static void
move_down(struct session *ses, struct document_view *doc_view, int type)
{
	int newpos;

	assert(ses && doc_view && doc_view->vs);
	if_assert_failed return;

	ses->navigate_mode = NAVIGATE_LINKWISE;

	newpos = doc_view->vs->y + doc_view->box.height;
	if (newpos < doc_view->document->height)
		doc_view->vs->y = newpos;

	if (current_link_is_visible(doc_view)) return;
	if (type)
		find_link_down(doc_view);
	else
		find_link_page_down(doc_view);
}

static void
move_page_down(struct session *ses, struct document_view *doc_view)
{
	int count = ses->kbdprefix.repeat_count;

	if (!count) count = 1;

	while (count--)
		move_down(ses, doc_view, 0);
}

/* type == 0 -> PAGE_UP
 * type == 1 -> UP */
static void
move_up(struct session *ses, struct document_view *doc_view, int type)
{
	assert(ses && doc_view && doc_view->vs);
	if_assert_failed return;

	if (doc_view->vs->y == 0) return;
	doc_view->vs->y -= doc_view->box.height;
	int_lower_bound(&doc_view->vs->y, 0);

	if (current_link_is_visible(doc_view)) return;

	if (type)
		find_link_up(doc_view);
	else
		find_link_page_up(doc_view);
}

static void
move_page_up(struct session *ses, struct document_view *doc_view)
{
	int count = ses->kbdprefix.repeat_count;

	if (!count) count = 1;

	while (count--)
		move_up(ses, doc_view, 0);
}

static void
move_link(struct session *ses, struct document_view *doc_view, int direction,
	  int wraparound_bound, int wraparound_link)
{
	int count;

	assert(ses && doc_view && doc_view->vs && doc_view->document);
	if_assert_failed return;

	ses->navigate_mode = NAVIGATE_LINKWISE;

	count = int_max(ses->kbdprefix.repeat_count, 1);

	while (count--) {
		int current_link = doc_view->vs->current_link;

		if (current_link == wraparound_bound
		    && get_opt_int("document.browse.links.wraparound")) {
			jump_to_link_number(ses, doc_view, wraparound_link);
			/* FIXME: This needs further work, we should call
			 * page_down() and set_textarea() under some conditions
			 * as well. --pasky */
			continue;
		}

		if (current_link != wraparound_bound
		    && next_in_view(doc_view, current_link + direction, direction, in_viewy, set_pos_x))
			continue;

		if (direction > 0) {
			move_down(ses, doc_view, 1);
		} else {
			move_up(ses, doc_view, 1);
		}

		if (current_link != wraparound_bound
		    && current_link != doc_view->vs->current_link) {
			set_textarea(doc_view, -direction);
		}
	}
}

#define move_link_next(ses, doc_view) move_link(ses, doc_view,  1, doc_view->document->nlinks - 1, 0)
#define move_link_prev(ses, doc_view) move_link(ses, doc_view, -1, 0, doc_view->document->nlinks - 1)

static void
move_link_dir(struct session *ses, struct document_view *doc_view, int dir_x, int dir_y)
{
	int count;

	assert(ses && doc_view && doc_view->vs && doc_view->document);
	if_assert_failed return;

	ses->navigate_mode = NAVIGATE_LINKWISE;
	count = int_max(ses->kbdprefix.repeat_count, 1);

	while (count--) {
		int current_link = doc_view->vs->current_link;

		if (next_in_dir(doc_view, current_link, dir_x, dir_y))
			continue;

		if (dir_y > 0)
			move_down(ses, doc_view, 1);
		else if (dir_y < 0)
			move_up(ses, doc_view, 1);

		if (dir_y && current_link != doc_view->vs->current_link) {
			set_textarea(doc_view, -dir_y);
		}
	}
}

#define move_link_up(ses, doc_view) move_link_dir(ses, doc_view,  0, -1)
#define move_link_down(ses, doc_view) move_link_dir(ses, doc_view,  0,  1)
#define move_link_left(ses, doc_view) move_link_dir(ses, doc_view, -1,  0)
#define move_link_right(ses, doc_view) move_link_dir(ses, doc_view,  1,  0)

/* @steps > 0 -> down */
static void
vertical_scroll(struct session *ses, struct document_view *doc_view, int steps)
{
	int y;

	assert(ses && doc_view && doc_view->vs && doc_view->document);
	if_assert_failed return;

	y = doc_view->vs->y + steps;
	if (steps > 0) {
		/* DOWN */
		int max_height = doc_view->document->height - doc_view->box.height;

		if (doc_view->vs->y >= max_height) return;
		int_upper_bound(&y, max_height);
	}

	int_lower_bound(&y, 0);

	if (doc_view->vs->y == y) return;

	doc_view->vs->y = y;

	if (current_link_is_visible(doc_view)) return;

	if (steps > 0)
		find_link_page_down(doc_view);
	else
		find_link_page_up(doc_view);
}

/* @steps > 0 -> right */
static void
horizontal_scroll(struct session *ses, struct document_view *doc_view, int steps)
{
	int x;

	assert(ses && doc_view && doc_view->vs && doc_view->document);
	if_assert_failed return;

	x = doc_view->vs->x + steps;
	int_bounds(&x, 0, doc_view->document->width - 1);
	if (doc_view->vs->x == x) return;

	doc_view->vs->x = x;

	if (current_link_is_visible(doc_view)) return;

	find_link_page_down(doc_view);
}

static void
scroll_up(struct session *ses, struct document_view *doc_view)
{
	int steps = ses->kbdprefix.repeat_count;

	if (!steps)
		steps = get_opt_int("document.browse.scrolling.vertical_step");

	vertical_scroll(ses, doc_view, -steps);
}

static void
scroll_down(struct session *ses, struct document_view *doc_view)
{
	int steps = ses->kbdprefix.repeat_count;

	if (!steps)
		steps = get_opt_int("document.browse.scrolling.vertical_step");

	vertical_scroll(ses, doc_view, steps);
}

static void
scroll_left(struct session *ses, struct document_view *doc_view)
{
	int steps = ses->kbdprefix.repeat_count;

	if (!steps)
		steps = get_opt_int("document.browse.scrolling.horizontal_step");

	horizontal_scroll(ses, doc_view, -steps);
}

static void
scroll_right(struct session *ses, struct document_view *doc_view)
{
	int steps = ses->kbdprefix.repeat_count;

	if (!steps)
		steps = get_opt_int("document.browse.scrolling.horizontal_step");

	horizontal_scroll(ses, doc_view, steps);
}

#ifdef CONFIG_MOUSE
static void
scroll_mouse_up(struct session *ses, struct document_view *doc_view)
{
	int steps = get_opt_int("document.browse.scrolling.vertical_step");

	vertical_scroll(ses, doc_view, -steps);
}

static void
scroll_mouse_down(struct session *ses, struct document_view *doc_view)
{
	int steps = get_opt_int("document.browse.scrolling.vertical_step");

	vertical_scroll(ses, doc_view, steps);
}

static void
scroll_mouse_left(struct session *ses, struct document_view *doc_view)
{
	int steps = get_opt_int("document.browse.scrolling.horizontal_step");

	horizontal_scroll(ses, doc_view, -steps);
}

static void
scroll_mouse_right(struct session *ses, struct document_view *doc_view)
{
	int steps = get_opt_int("document.browse.scrolling.horizontal_step");

	horizontal_scroll(ses, doc_view, steps);
}
#endif /* CONFIG_MOUSE */

static enum frame_event_status move_cursor(struct session *ses,
					   struct document_view *doc_view,
					   int x, int y);

static void
move_document_start(struct session *ses, struct document_view *doc_view)
{
	assert(ses && doc_view && doc_view->vs);
	if_assert_failed return;

	doc_view->vs->y = doc_view->vs->x = 0;

	if (ses->navigate_mode == NAVIGATE_CURSOR_ROUTING) {
		/* Move to the first line and the first column. */
		move_cursor(ses, doc_view, doc_view->box.x, doc_view->box.y);
	} else {
		find_link_page_down(doc_view);
	}
}

static void
move_document_end(struct session *ses, struct document_view *doc_view)
{
	int max_height;

	assert(ses && doc_view && doc_view->vs && doc_view->document);
	if_assert_failed return;

	max_height = doc_view->document->height - doc_view->box.height;
	doc_view->vs->x = 0;
	int_lower_bound(&doc_view->vs->y, int_max(0, max_height));

	if (ses->navigate_mode == NAVIGATE_CURSOR_ROUTING) {
		/* Move to the last line of the document,
		 * but preserve the column. This is done to avoid
		 * moving the cursor backwards if it is already
		 * on the last line but is not on the first column. */
		move_cursor(ses, doc_view, ses->tab->x,
			    doc_view->document->height - doc_view->vs->y);
	} else {
		find_link_page_up(doc_view);
	}
}

void
set_frame(struct session *ses, struct document_view *doc_view, int xxxx)
{
	assert(ses && ses->doc_view && doc_view && doc_view->vs);
	if_assert_failed return;

	if (doc_view == ses->doc_view) return;
	goto_uri(ses, doc_view->vs->uri);
	ses->navigate_mode = NAVIGATE_LINKWISE;
}


void
toggle_plain_html(struct session *ses, struct document_view *doc_view, int xxxx)
{
	assert(ses && doc_view && ses->tab && ses->tab->term);
	if_assert_failed return;

	if (!doc_view->vs) {
		nowhere_box(ses->tab->term, NULL);
		return;
	}

	doc_view->vs->plain = !doc_view->vs->plain;
	draw_formatted(ses, 1);
}

void
toggle_wrap_text(struct session *ses, struct document_view *doc_view, int xxxx)
{
	assert(ses && doc_view && ses->tab && ses->tab->term);
	if_assert_failed return;

	if (!doc_view->vs) {
		nowhere_box(ses->tab->term, NULL);
		return;
	}

	doc_view->vs->wrap = !doc_view->vs->wrap;
	draw_formatted(ses, 1);
}

/* Move the cursor to the document coordinates provided as @x and @y,
 * scroll the document if necessary, put us in cursor-routing navigation mode
 * if that is not the current mode, and select any link under the cursor. */
static enum frame_event_status
move_cursor(struct session *ses, struct document_view *doc_view, int x, int y)
{
	struct terminal *term = ses->tab->term;
	struct box *box = &doc_view->box;
	struct link *link;

	/* If cursor was moved outside the document view scroll it, but only
	 * within the document canvas */
	if (!is_in_box(box, x, y)) {
		int vs_x = doc_view->vs->x, vs_y = doc_view->vs->y;
		int max_height = doc_view->document->height - doc_view->vs->y;
		int max_width = doc_view->document->width - doc_view->vs->x;

		if (y < box->y) {
			vertical_scroll(ses, doc_view, -1);

		} else if (y >= box->y + box->height && y <= max_height) {
			vertical_scroll(ses, doc_view, 1);

		} else if (x < box->x) {
			horizontal_scroll(ses, doc_view, -1);

		} else if (x >= box->x + box->width && x <= max_width) {
			horizontal_scroll(ses, doc_view, 1);
		}

		/* If the view was not scrolled there's nothing more to do */
		if (vs_x == doc_view->vs->x && vs_y == doc_view->vs->y)
			return FRAME_EVENT_OK;

		/* Restrict the cursor position within the current view */
		int_bounds(&x, box->x, box->x + box->width - 1);
		int_bounds(&y, box->y, box->y + box->height - 1);
	}

	/* Scrolling could have changed the navigation mode */
	ses->navigate_mode = NAVIGATE_CURSOR_ROUTING;

	link = get_link_at_coordinates(doc_view, x - box->x, y - box->y);
	if (link) {
		doc_view->vs->current_link = link - doc_view->document->links;
	} else {
		doc_view->vs->current_link = -1;
	}

	/* Set the unblockable cursor position and update the window pointer so
	 * stuff like the link menu will be drawn relative to the cursor. */
	set_cursor(term, x, y, 0);
	set_window_ptr(ses->tab, x, y);

	return FRAME_EVENT_REFRESH;
}

#define move_cursor_left(ses, view)	move_cursor(ses, view, ses->tab->x - 1, ses->tab->y)
#define move_cursor_right(ses, view)	move_cursor(ses, view, ses->tab->x + 1, ses->tab->y)
#define move_cursor_up(ses, view)	move_cursor(ses, view, ses->tab->x, ses->tab->y - 1)
#define move_cursor_down(ses, view)	move_cursor(ses, view, ses->tab->x, ses->tab->y + 1)


static int
try_jump_to_link_number(struct session *ses, struct document_view *doc_view)
{
	int link_number = ses->kbdprefix.repeat_count - 1;

	if (link_number < 0) return 0;

	ses->kbdprefix.repeat_count = 0;
	if (link_number >= doc_view->document->nlinks)
		return 1;

	jump_to_link_number(ses, doc_view, link_number);
	refresh_view(ses, doc_view, 0);

	return 0;
}

static enum frame_event_status
frame_ev_kbd_number(struct session *ses, struct document_view *doc_view,
		    struct term_event *ev)
{
	struct document *document = doc_view->document;
	struct document_options *doc_opts = &document->options;

	if (ev->y
	    || !doc_opts->num_links_key
	    || (doc_opts->num_links_key == 1 && !doc_opts->num_links_display)) {
		/* Repeat count.
		 * ses->kbdprefix.repeat_count is initialized to zero
		 * the first time by init_session() calloc() call.
		 * When used, it has to be reset to zero. */

		ses->kbdprefix.repeat_count *= 10;
		ses->kbdprefix.repeat_count += ev->x - '0';

		/* If too big, just restart from zero, so pressing
		 * '0' six times or more will reset the count. */
		if (ses->kbdprefix.repeat_count > 65536)
			ses->kbdprefix.repeat_count = 0;

		return FRAME_EVENT_OK;
	}

	if (ev->x >= '1' && !ev->y) {
		int nlinks = document->nlinks, length;
		unsigned char d[2] = { ev->x, 0 };

		ses->kbdprefix.repeat_count = 0;

		if (!nlinks) return FRAME_EVENT_OK;

		for (length = 1; nlinks; nlinks /= 10)
			length++;

		input_field(ses->tab->term, NULL, 1,
			    N_("Go to link"), N_("Enter link number"),
			    N_("OK"), N_("Cancel"), ses, NULL,
			    length, d, 1, document->nlinks, check_number,
			    (void (*)(void *, unsigned char *)) goto_link_number, NULL);

		return FRAME_EVENT_OK;
	}

	return FRAME_EVENT_IGNORED;
}

static enum frame_event_status
frame_ev_kbd(struct session *ses, struct document_view *doc_view, struct term_event *ev)
{
	enum frame_event_status status = FRAME_EVENT_REFRESH;

#ifdef CONFIG_MARKS
	if (ses->kbdprefix.mark != KP_MARK_NOTHING) {
		/* Marks */
		unsigned char mark = ev->x;

		switch (ses->kbdprefix.mark) {
			case KP_MARK_NOTHING:
				assert(0);
				break;

			case KP_MARK_SET:
				/* It is intentional to set the mark
				 * to NULL if !doc_view->vs. */
				set_mark(mark, doc_view->vs);
				break;

			case KP_MARK_GOTO:
				goto_mark(mark, doc_view->vs);
				break;
		}

		ses->kbdprefix.repeat_count = 0;
		ses->kbdprefix.mark = KP_MARK_NOTHING;
		return FRAME_EVENT_REFRESH;
	}
#endif

	if (get_opt_int("document.browse.accesskey.priority") >= 2
	    && try_document_key(ses, doc_view, ev)) {
		/* The document ate the key! */
		return FRAME_EVENT_REFRESH;
	}

	switch (kbd_action(KM_MAIN, ev, NULL)) {
		case ACT_MAIN_MOVE_PAGE_DOWN: move_page_down(ses, doc_view); break;
		case ACT_MAIN_MOVE_PAGE_UP: move_page_up(ses, doc_view); break;
		case ACT_MAIN_MOVE_LINK_NEXT: move_link_next(ses, doc_view); break;
		case ACT_MAIN_MOVE_LINK_PREV: move_link_prev(ses, doc_view); break;
		case ACT_MAIN_MOVE_LINK_UP: move_link_up(ses, doc_view); break;
		case ACT_MAIN_MOVE_LINK_DOWN: move_link_down(ses, doc_view); break;
		case ACT_MAIN_MOVE_LINK_LEFT: move_link_left(ses, doc_view); break;
		case ACT_MAIN_MOVE_LINK_RIGHT: move_link_right(ses, doc_view); break;
		case ACT_MAIN_MOVE_DOCUMENT_START: move_document_start(ses, doc_view); break;
		case ACT_MAIN_MOVE_DOCUMENT_END: move_document_end(ses, doc_view); break;

		case ACT_MAIN_SCROLL_DOWN: scroll_down(ses, doc_view); break;
		case ACT_MAIN_SCROLL_UP: scroll_up(ses, doc_view); break;
		case ACT_MAIN_SCROLL_LEFT: scroll_left(ses, doc_view); break;
		case ACT_MAIN_SCROLL_RIGHT: scroll_right(ses, doc_view); break;

		case ACT_MAIN_MOVE_CURSOR_UP:
			status = move_cursor_up(ses, doc_view);
			break;
		case ACT_MAIN_MOVE_CURSOR_DOWN:
			status = move_cursor_down(ses, doc_view);
			break;
		case ACT_MAIN_MOVE_CURSOR_LEFT:
			status = move_cursor_left(ses, doc_view);
			break;
		case ACT_MAIN_MOVE_CURSOR_RIGHT:
			status = move_cursor_right(ses, doc_view);
			break;

		case ACT_MAIN_COPY_CLIPBOARD: {
			/* This looks bogus. Why print_current_link()
			 * it adds all kins of stuff that is not part
			 * of the current link. I'd propose to use
			 * get_link_uri() or something. --jonas */
			char *current_link;

			if (try_jump_to_link_number(ses, doc_view))
				return FRAME_EVENT_OK;

			current_link = get_current_link_info(ses, doc_view);

			if (current_link) {
				set_clipboard_text(current_link);
				mem_free(current_link);
			}
			break;
		}

		case ACT_MAIN_LINK_FOLLOW:
			if (try_jump_to_link_number(ses, doc_view))
				status = FRAME_EVENT_OK;
			else
				status = enter(ses, doc_view, 0);
			break;
		case ACT_MAIN_LINK_FOLLOW_RELOAD:
			if (try_jump_to_link_number(ses, doc_view))
				status = FRAME_EVENT_OK;
			else
				status = enter(ses, doc_view, 1);
			break;
		case ACT_MAIN_JUMP_TO_LINK:
			try_jump_to_link_number(ses, doc_view);
			status = FRAME_EVENT_OK;
			break;
		case ACT_MAIN_MARK_SET:
#ifdef CONFIG_MARKS
			ses->kbdprefix.mark = KP_MARK_SET;
#endif
			status = FRAME_EVENT_OK;
			break;
		case ACT_MAIN_MARK_GOTO:
#ifdef CONFIG_MARKS
			/* TODO: Show promptly a menu (or even listbox?)
			 * with all the marks. But the next letter must
			 * still choose a mark directly! --pasky */
			ses->kbdprefix.mark = KP_MARK_GOTO;
#endif
			status = FRAME_EVENT_OK;
			break;
		case ACT_MAIN_LINK_DOWNLOAD:
		case ACT_MAIN_LINK_DOWNLOAD_RESUME:
		case ACT_MAIN_LINK_DOWNLOAD_IMAGE:
		case ACT_MAIN_LINK_MENU:
		case ACT_MAIN_VIEW_IMAGE:
		case ACT_MAIN_OPEN_LINK_IN_NEW_WINDOW:
		case ACT_MAIN_OPEN_LINK_IN_NEW_TAB:
		case ACT_MAIN_OPEN_LINK_IN_NEW_TAB_IN_BACKGROUND:
			if (try_jump_to_link_number(ses, doc_view))
				status = FRAME_EVENT_OK;
			else
				status = FRAME_EVENT_IGNORED;
			break;
		default:
			if (ev->x >= '0' && ev->x <= '9') {
				status = frame_ev_kbd_number(ses, doc_view,
							     ev);

				if (status != FRAME_EVENT_IGNORED)
					return status;
			}

			if (get_opt_int("document.browse.accesskey.priority") == 1
				   && try_document_key(ses, doc_view, ev)) {
				/* The document ate the key! */
				status = FRAME_EVENT_REFRESH;

			} else {
				status = FRAME_EVENT_IGNORED;
			}
	}

	ses->kbdprefix.repeat_count = 0;
	return status;
}

#ifdef CONFIG_MOUSE
static enum frame_event_status
frame_ev_mouse(struct session *ses, struct document_view *doc_view, struct term_event *ev)
{
	enum frame_event_status status = FRAME_EVENT_REFRESH;
	struct link *link = get_link_at_coordinates(doc_view, ev->x, ev->y);

	if (check_mouse_wheel(ev)) {
		if (!check_mouse_action(ev, B_DOWN)) {
			/* We handle only B_DOWN case... */
		} else if (check_mouse_button(ev, B_WHEEL_UP)) {
			scroll_mouse_up(ses, doc_view);
		} else if (check_mouse_button(ev, B_WHEEL_DOWN)) {
			scroll_mouse_down(ses, doc_view);
		}

	} else if (link) {
		doc_view->vs->current_link = link - doc_view->document->links;

		if (!link_is_textinput(link)
		    && check_mouse_action(ev, B_UP)) {

			status = FRAME_EVENT_OK;

			refresh_view(ses, doc_view, 0);

			if (check_mouse_button(ev, B_LEFT))
				status = enter(ses, doc_view, 0);
			else if (check_mouse_button(ev, B_MIDDLE))
				open_current_link_in_new_tab(ses, 1);
			else
				link_menu(ses->tab->term, NULL, ses);
		}

	} else if (check_mouse_button(ev, B_LEFT)) {
		/* Clicking the edge of screen will scroll the document. */

		int scrollmargin = get_opt_int("document.browse.scrolling.margin");

		/* XXX: This is code duplication with kbd handlers. But
		 * repeatcount-free here. */

		if (ev->y < scrollmargin) {
			scroll_mouse_up(ses, doc_view);
		}
		if (ev->y >= doc_view->box.height - scrollmargin) {
			scroll_mouse_down(ses, doc_view);
		}

		if (ev->x < scrollmargin * 2) {
			scroll_mouse_left(ses, doc_view);
		}
		if (ev->x >= doc_view->box.width - scrollmargin * 2) {
			scroll_mouse_right(ses, doc_view);
		}

	} else {
		status = FRAME_EVENT_IGNORED;
	}

	return status;
}
#endif /* CONFIG_MOUSE */

static enum frame_event_status
frame_ev(struct session *ses, struct document_view *doc_view, struct term_event *ev)
{
	struct link *link;
	enum frame_event_status status;

	assert(ses && doc_view && doc_view->document && doc_view->vs && ev);
	if_assert_failed return FRAME_EVENT_IGNORED;

	link = get_current_link(doc_view);

	if (link && link_is_textinput(link)) {
	    status = field_op(ses, doc_view, link, ev);

	    if (status != FRAME_EVENT_IGNORED)
		    return status;
	}

	if (ev->ev == EV_KBD) {
		status = frame_ev_kbd(ses, doc_view, ev);

#ifdef CONFIG_MOUSE
	} else if (ev->ev == EV_MOUSE) {
		status = frame_ev_mouse(ses, doc_view, ev);
#endif /* CONFIG_MOUSE */

	} else {
		status = FRAME_EVENT_IGNORED;
	}

	if (ses->insert_mode == INSERT_MODE_ON
	    && link != get_current_link(doc_view))
		ses->insert_mode = INSERT_MODE_OFF;

	return status;
}

struct document_view *
current_frame(struct session *ses)
{
	struct document_view *doc_view = NULL;
	int current_frame_number;

	assert(ses);
	if_assert_failed return NULL;

	if (!have_location(ses)) return NULL;

	current_frame_number = cur_loc(ses)->vs.current_link;
	if (current_frame_number == -1) current_frame_number = 0;

	foreach (doc_view, ses->scrn_frames) {
		if (document_has_frames(doc_view->document)) continue;
		if (!current_frame_number--) return doc_view;
	}

	doc_view = ses->doc_view;

	assert(doc_view && doc_view->document);
	if_assert_failed return NULL;

	if (document_has_frames(doc_view->document)) return NULL;
	return doc_view;
}

static enum frame_event_status
send_to_frame(struct session *ses, struct term_event *ev)
{
	struct document_view *doc_view;
	enum frame_event_status status;

	assert(ses && ses->tab && ses->tab->term && ev);
	if_assert_failed return FRAME_EVENT_IGNORED;
	doc_view = current_frame(ses);
	assertm(doc_view, "document not formatted");
	if_assert_failed return FRAME_EVENT_IGNORED;

	status = frame_ev(ses, doc_view, ev);

	if (status == FRAME_EVENT_REFRESH)
		refresh_view(ses, doc_view, 0);

	return status;
}

#ifdef CONFIG_MOUSE
static int
do_mouse_event(struct session *ses, struct term_event *ev,
	       struct document_view *doc_view)
{
	struct term_event evv;
	struct document_view *matched = NULL, *first = doc_view;

	assert(ses && ev);
	if_assert_failed return 0;

	do {
		struct document_options *o = &doc_view->document->options;

		assert(doc_view && doc_view->document);
		if_assert_failed return 0;

		/* FIXME: is_in_box() ? */
		if (ev->x >= o->box.x && ev->x < o->box.x + doc_view->box.width
		    && ev->y >= o->box.y && ev->y < o->box.y + doc_view->box.height) {
			matched = doc_view;
			break;
		}

		next_frame(ses, 1);
		doc_view = current_frame(ses);

	} while (doc_view != first);

	if (!matched) return 0;

	if (doc_view != first) draw_formatted(ses, 0);

	memcpy(&evv, ev, sizeof(struct term_event));
	evv.x -= doc_view->box.x;
	evv.y -= doc_view->box.y;
	return send_to_frame(ses, &evv);
}
#endif /* CONFIG_MOUSE */

void
send_event(struct session *ses, struct term_event *ev)
{
	struct document_view *doc_view;

	assert(ses && ev);
	if_assert_failed return;
	doc_view = current_frame(ses);

	if (ev->ev == EV_KBD) {
		int func_ref;
		enum main_action action;

		if (doc_view && send_to_frame(ses, ev) != FRAME_EVENT_IGNORED)
			return;

		action = kbd_action(KM_MAIN, ev, &func_ref);

		if (action == ACT_MAIN_QUIT) {
quit:
			if (ev->x == KBD_CTRL_C)
				action = ACT_MAIN_REALLY_QUIT;
		}

		if (do_action(ses, action, 0) == action) {
			/* Did the session disappear in some EV_ABORT handler? */
			if (action == ACT_MAIN_TAB_CLOSE
			    || action == ACT_MAIN_TAB_CLOSE_ALL_BUT_CURRENT)
				ses = NULL;
			goto x;
		}

		switch (action) {
			case ACT_MAIN_SCRIPTING_FUNCTION:
#ifdef CONFIG_SCRIPTING
				trigger_event(func_ref, ses);
#endif
				break;

			default:
				if (ev->x == KBD_CTRL_C) goto quit;
				if (ev->y & KBD_ALT) {
					struct window *m;

					ev->y &= ~KBD_ALT;
					activate_bfu_technology(ses, -1);
					m = ses->tab->term->windows.next;
					m->handler(m, ev, 0);
					if (ses->tab->term->windows.next == m) {
						delete_window(m);

					} else if (doc_view
						   && get_opt_int("document"
								  ".browse"
								  ".accesskey"
								  ".priority")
						    <= 0
						   && try_document_key(ses,
							   doc_view, ev)) {
						/* The document ate the key! */
						refresh_view(ses, doc_view, 0);
						return;
					} else {
						goto x;
					}
					ev->y |= ~KBD_ALT;
				}
		}
	}
#ifdef CONFIG_MOUSE
	if (ev->ev == EV_MOUSE) {
		int bars;
		int x = 0;

		if (ev->y == 0
		    && check_mouse_action(ev, B_DOWN)
		    && !check_mouse_wheel(ev)) {
			struct window *m;

			activate_bfu_technology(ses, -1);
			m = ses->tab->term->windows.next;
			m->handler(m, ev, 0);
			goto x;
		}

		bars = 0;
		if (ses->status.show_tabs_bar) bars++;
		if (ses->status.show_status_bar) bars++;

		if (ev->y == ses->tab->term->height - bars && check_mouse_action(ev, B_DOWN)) {
			int tab = get_tab_number_by_xpos(ses->tab->term, ev->x);

			if (check_mouse_button(ev, B_WHEEL_UP)) {
				switch_to_prev_tab(ses->tab->term);

			} else if (check_mouse_button(ev, B_WHEEL_DOWN)) {
				switch_to_next_tab(ses->tab->term);

			} else if (tab != -1) {
				switch_to_tab(ses->tab->term, tab, -1);

				if (check_mouse_button(ev, B_RIGHT)) {
					struct window *tab = get_current_tab(ses->tab->term);

					set_window_ptr(tab, ev->x, ev->y);
					tab_menu(ses->tab->term, tab, tab->data);
				}
			}

			goto x;
		}

		if (doc_view) x = do_mouse_event(ses, ev, doc_view);

		if (!x && check_mouse_button(ev, B_RIGHT)) {
			set_window_ptr(ses->tab, ev->x, ev->y);
			tab_menu(ses->tab->term, ses->tab, ses);
		}
	}
#endif /* CONFIG_MOUSE */
	return;

x:
	/* ses may disappear ie. in close_tab() */
	if (ses) ses->kbdprefix.repeat_count = 0;
}

void
download_link(struct session *ses, struct document_view *doc_view, int action)
{
	struct link *link = get_current_link(doc_view);
	void (*download)(void *ses, unsigned char *file) = start_download;

	if (!link) return;

	if (ses->download_uri) {
		done_uri(ses->download_uri);
		ses->download_uri = NULL;
	}

	switch (action) {
		case ACT_MAIN_LINK_DOWNLOAD_RESUME:
			download = resume_download;
		case ACT_MAIN_LINK_DOWNLOAD:
			ses->download_uri = get_link_uri(ses, doc_view, link);
			break;

		case ACT_MAIN_LINK_DOWNLOAD_IMAGE:
			if (!link->where_img) break;
			ses->download_uri = get_uri(link->where_img, 0);
			break;

		default:
			INTERNAL("I think you forgot to take your medication, mental boy!");
			return;
	}

	if (!ses->download_uri) return;

	set_session_referrer(ses, doc_view->document->uri);
	query_file(ses, ses->download_uri, ses, download, NULL, 1);
}

void
view_image(struct session *ses, struct document_view *doc_view, int xxxx)
{
	struct link *link = get_current_link(doc_view);

	if (link && link->where_img)
		goto_url(ses, link->where_img);
}

void
save_as(struct session *ses, struct document_view *doc_view, int magic)
{
	assert(ses);
	if_assert_failed return;

	if (!have_location(ses)) return;

	if (ses->download_uri) done_uri(ses->download_uri);
	ses->download_uri = get_uri_reference(cur_loc(ses)->vs.uri);

	assert(doc_view && doc_view->document && doc_view->document->uri);
	if_assert_failed return;

	set_session_referrer(ses, doc_view->document->uri);
	query_file(ses, ses->download_uri, ses, start_download, NULL, 1);
}

static void
save_formatted_finish(struct terminal *term, int h, void *data, int resume)
{
	struct document *document = data;

	assert(term && document);
	if_assert_failed return;

	if (h == -1) return;
	if (dump_to_file(document, h)) {
		msg_box(term, NULL, 0,
			N_("Save error"), AL_CENTER,
			N_("Error writing to file"),
			NULL, 1,
			N_("OK"), NULL, B_ENTER | B_ESC);
	}
	close(h);
}

static void
save_formatted(void *data, unsigned char *file)
{
	struct session *ses = data;
	struct document_view *doc_view;

	assert(ses && ses->tab && ses->tab->term && file);
	if_assert_failed return;
	doc_view = current_frame(ses);
	assert(doc_view && doc_view->document);
	if_assert_failed return;

	create_download_file(ses->tab->term, file, NULL, 0, 0,
			     save_formatted_finish, doc_view->document);
}

void
save_formatted_dlg(struct session *ses, struct document_view *doc_view, int xxxx)
{
	query_file(ses, doc_view->vs->uri, ses, save_formatted, NULL, 1);
}
