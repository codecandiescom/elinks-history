/* HTML viewer (and much more) */
/* $Id: view.c,v 1.477 2004/06/16 09:15:58 miciah Exp $ */

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

static inline int
find_tag(struct document *document, unsigned char *name, int namelen)
{
	struct tag *tag;

	foreach (tag, document->tags)
		if (!strlcasecmp(tag->name, -1, name, namelen))
			return tag->y;

	return -1;
}

static void
draw_frame_lines(struct terminal *term, struct frameset_desc *frameset_desc,
		 int xp, int yp)
{
	/* Optionalize? */
	struct color_pair colors = INIT_COLOR_PAIR(0x000000, 0xCCCCCC);
	register int y, j;

	assert(term && frameset_desc && frameset_desc->frame_desc);
	if_assert_failed return;

	y = yp - 1;
	for (j = 0; j < frameset_desc->box.height; j++) {
		register int x, i;
		int height = frameset_desc->frame_desc[j * frameset_desc->box.width].height;

		x = xp - 1;
		for (i = 0; i < frameset_desc->box.width; i++) {
			int width = frameset_desc->frame_desc[i].width;

			if (i) {
				struct box box;

				set_box(&box, x, y + 1, 1, height);
				draw_box(term, &box, BORDER_SVLINE, SCREEN_ATTR_FRAME, &colors);

				if (j == frameset_desc->box.height - 1)
					draw_border_cross(term, x, y + height + 1,
							  BORDER_X_UP, &colors);
			} else if (j) {
				if (x >= 0)
					draw_border_cross(term, x, y,
							  BORDER_X_RIGHT, &colors);
			}

			if (j) {
				struct box box;

				set_box(&box, x + 1, y, width, 1);
				draw_box(term, &box, BORDER_SHLINE, SCREEN_ATTR_FRAME, &colors);

				if (i == frameset_desc->box.width - 1
				    && x + width + 1 < term->width)
					draw_border_cross(term, x + width + 1, y,
							  BORDER_X_LEFT, &colors);
			} else if (i) {
				draw_border_cross(term, x, y, BORDER_X_DOWN, &colors);
			}

			if (i && j)
				draw_border_char(term, x, y, BORDER_SCROSS, &colors);

			x += width + 1;
		}
		y += height + 1;
	}

	y = yp - 1;
	for (j = 0; j < frameset_desc->box.height; j++) {
		register int x, i;
		int pj = j * frameset_desc->box.width;
		int height = frameset_desc->frame_desc[pj].height;

		x = xp - 1;
		for (i = 0; i < frameset_desc->box.width; i++) {
			int width = frameset_desc->frame_desc[i].width;
			int p = pj + i;

			if (frameset_desc->frame_desc[p].subframe) {
				draw_frame_lines(term, frameset_desc->frame_desc[p].subframe,
						 x + 1, y + 1);
			}
			x += width + 1;
		}
		y += height + 1;
	}
}

static void
draw_view_status(struct session *ses, struct document_view *doc_view, int active)
{
	struct terminal *term = ses->tab->term;

	draw_forms(term, doc_view);
	draw_searched(term, doc_view);
	if (active) draw_current_link(ses, doc_view);
}

void
draw_doc(struct session *ses, struct document_view *doc_view, int active)
{
	struct color_pair color = INIT_COLOR_PAIR(0, 0);
	struct view_state *vs;
	struct terminal *term;
	struct box *box;
	int vx, vy;
	int y;

	assert(ses && ses->tab && ses->tab->term && doc_view);
	if_assert_failed return;

	box = &doc_view->box;
	term = ses->tab->term;

	/* The code in this function assumes that both width and height are
	 * bigger than 1 so we have to bail out here. */
	if (box->width < 2 || box->height < 2) return;

	if (active) {
		set_cursor(term, box->x + box->width - 1, box->y + box->height - 1, 1);
		set_window_ptr(get_current_tab(term), box->x, box->y);
	}

	if (doc_view->document->height)
		color.background = doc_view->document->bgcolor;

	if (!doc_view->vs) {
		draw_box(term, box, ' ', 0, &color);
		return;
	}

	if (document_has_frames(doc_view->document)) {
	 	draw_box(term, box, ' ', 0, &color);
		draw_frame_lines(term, doc_view->document->frame_desc, box->x, box->y);
		if (doc_view->vs && doc_view->vs->current_link == -1)
			doc_view->vs->current_link = 0;
		return;
	}
	check_vs(doc_view);
	vs = doc_view->vs;

	if (!vs->did_fragment) {
		unsigned char *tag = vs->uri->fragment;
		int taglen = vs->uri->fragmentlen;

		vy = find_tag(doc_view->document, tag, taglen);

		switch (vy) {
		case -1:
		{
			struct cache_entry *cached = find_in_cache(doc_view->document->uri);

			if (!cached || cached->incomplete)
				break;

			vs->did_fragment = 1;
			tag = memacpy(tag, taglen);

			msg_box(term, NULL, MSGBOX_FREE_TEXT,
				N_("Missing fragment"), AL_CENTER,
				msg_text(term, N_("The requested fragment "
					"\"#%s\" doesn't exist."),
					tag),
				NULL, 1,
				N_("OK"), NULL, B_ENTER | B_ESC);

			mem_free_if(tag);
			break;
		}

		default:
			int_bounds(&vy, 0, doc_view->document->height - 1);
			vs->y = vy;
			set_link(doc_view);
			vs->did_fragment = 1;
		}
	}
	vx = vs->x;
	vy = vs->y;
	if (doc_view->last_x != -1
	    && doc_view->last_x == vx
	    && doc_view->last_y == vy
	    && !has_search_word(doc_view)) {
		clear_link(term, doc_view);
		draw_view_status(ses, doc_view, active);
		return;
	}
	free_link(doc_view);
	doc_view->last_x = vx;
	doc_view->last_y = vy;
	draw_box(term, box, ' ', 0, &color);
	if (!doc_view->document->height) return;

	while (vs->y >= doc_view->document->height) vs->y -= box->height;
	int_lower_bound(&vs->y, 0);
	if (vy != vs->y) vy = vs->y, check_vs(doc_view);
	for (y = int_max(vy, 0);
	     y < int_min(doc_view->document->height, box->height + vy);
	     y++) {
		int st = int_max(vx, 0);
		int en = int_min(doc_view->document->data[y].length,
				 box->width + vx);

		if (en - st <= 0) continue;
		draw_line(term, box->x + st - vx, box->y + y - vy, en - st,
			  &doc_view->document->data[y].chars[st]);
	}
	draw_view_status(ses, doc_view, active);
	if (has_search_word(doc_view))
		doc_view->last_x = doc_view->last_y = -1;
}

static void
draw_frames(struct session *ses)
{
	struct document_view *doc_view, *current_doc_view;
	int *l;
	int n, d, more;

	assert(ses && ses->doc_view && ses->doc_view->document);
	if_assert_failed return;

	if (!document_has_frames(ses->doc_view->document)) return;

	n = 0;
	foreach (doc_view, ses->scrn_frames) {
	       doc_view->last_x = doc_view->last_y = -1;
	       n++;
	}
	l = &cur_loc(ses)->vs.current_link;
	*l = int_max(*l, 0) % int_max(n, 1);

	current_doc_view = current_frame(ses);
	d = 0;
	do {
		more = 0;
		foreach (doc_view, ses->scrn_frames) {
			if (doc_view->depth == d)
				draw_doc(ses, doc_view, doc_view == current_doc_view);
			else if (doc_view->depth > d)
				more = 1;
		}
		d++;
	} while (more);
}

void
draw_formatted(struct session *ses, int rerender)
{
	assert(ses && ses->tab);
	if_assert_failed return;

	if (rerender) render_document_frames(ses);

	if (ses->tab != get_current_tab(ses->tab->term))
		return;

	if (!ses->doc_view || !ses->doc_view->document) {
		/*INTERNAL("document not formatted");*/
		struct box box;

		set_box(&box, 0, 1,
			ses->tab->term->width,
			ses->tab->term->height - 2);
		draw_box(ses->tab->term, &box, ' ', 0, NULL);
		return;
	}

	if (!ses->doc_view->vs && have_location(ses))
		ses->doc_view->vs = &cur_loc(ses)->vs;
	ses->doc_view->last_x = ses->doc_view->last_y = -1;

	refresh_view(ses, ses->doc_view, 1);
}

/* type == 0 -> PAGE_DOWN
 * type == 1 -> DOWN */
static void
move_down(struct session *ses, struct document_view *doc_view, int type)
{
	int newpos;

	assert(ses && doc_view && doc_view->vs);
	if_assert_failed return;

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
page_down(struct session *ses, struct document_view *doc_view)
{
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
page_up(struct session *ses, struct document_view *doc_view)
{
	move_up(ses, doc_view, 0);
}

void
down(struct session *ses, struct document_view *doc_view)
{
	int current_link;

	assert(ses && doc_view && doc_view->vs && doc_view->document);
	if_assert_failed return;

	current_link = doc_view->vs->current_link;

	if (current_link == doc_view->document->nlinks - 1) {
		if (get_opt_int("document.browse.links.wraparound")) {
			jump_to_link_number(ses, doc_view, 0);
			/* FIXME: This needs further work, we should call
			 * page_down() and set_textarea() under some conditions
			 * as well. --pasky */
			return;
		}
		current_link = -1;
	}

	if (current_link == -1
	    || !next_in_view(doc_view, current_link + 1, 1, in_viewy, set_pos_x)) {
		move_down(ses, doc_view, 1);
	}

	if (current_link != doc_view->vs->current_link) {
		set_textarea(ses, doc_view, KBD_UP);
	}
}

static void
up(struct session *ses, struct document_view *doc_view)
{
	int current_link;

	assert(ses && doc_view && doc_view->vs && doc_view->document);
	if_assert_failed return;

	current_link = doc_view->vs->current_link;

	if (current_link == 0) {
		if (get_opt_int("document.browse.links.wraparound")) {
	   		jump_to_link_number(ses, doc_view, doc_view->document->nlinks - 1);
			/* FIXME: This needs further work, we should call
			 * page_down() and set_textarea() under some conditions
			 * as well. --pasky */
			return;
		}
		current_link = -1;
	}

	if (current_link == -1
	    || !next_in_view(doc_view, current_link - 1, -1, in_viewy, set_pos_x)) {
		move_up(ses, doc_view, 1);
	}

	if (current_link != doc_view->vs->current_link) {
		set_textarea(ses, doc_view, KBD_DOWN);
	}
}

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

		int_upper_bound(&y, int_max(0, max_height));
	} else {
		/* UP */
		int_lower_bound(&y, 0);
	}

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
	if (steps > 0) {
		/* RIGHT */
		int max_width = doc_view->document->width - doc_view->box.width;

		int_upper_bound(&x, int_max(0, max_width));
	} else {
		/* LEFT */
		int_lower_bound(&x, 0);
	}

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

static void
home(struct session *ses, struct document_view *doc_view)
{
	assert(ses && doc_view && doc_view->vs);
	if_assert_failed return;

	doc_view->vs->y = doc_view->vs->x = 0;
	find_link_page_down(doc_view);
}

static void
x_end(struct session *ses, struct document_view *doc_view)
{
	int max_height;

	assert(ses && doc_view && doc_view->vs && doc_view->document);
	if_assert_failed return;

	max_height = doc_view->document->height - doc_view->box.height;
	doc_view->vs->x = 0;
	int_lower_bound(&doc_view->vs->y, int_max(0, max_height));
	find_link_page_up(doc_view);
}

void
set_frame(struct session *ses, struct document_view *doc_view, int a)
{
	assert(ses && ses->doc_view && doc_view && doc_view->vs);
	if_assert_failed return;

	if (doc_view == ses->doc_view) return;
	goto_uri(ses, doc_view->vs->uri);
}


void
toggle_plain_html(struct session *ses, struct document_view *doc_view, int a)
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
toggle_wrap_text(struct session *ses, struct document_view *doc_view, int a)
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

static inline void
rep_ev(struct session *ses, struct document_view *doc_view,
       void (*f)(struct session *, struct document_view *))
{
	register int i = 1;

	assert(ses && doc_view && f);
	if_assert_failed return;

	if (ses->kbdprefix.repeat_count) {
		i = ses->kbdprefix.repeat_count;
		ses->kbdprefix.repeat_count = 0;
	}

	while (i--) f(ses, doc_view);

}

int
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

	if (ev->x >= '0' && ev->x <= '9'
	    && (ev->y
		|| !doc_view->document->options.num_links_key
		|| (doc_view->document->options.num_links_key == 1
		    && !doc_view->document->options.num_links_display))) {
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

	if (get_opt_int("document.browse.accesskey.priority") >= 2
	    && try_document_key(ses, doc_view, ev)) {
		/* The document ate the key! */
		return FRAME_EVENT_REFRESH;
	}

	switch (kbd_action(KM_MAIN, ev, NULL)) {
		case ACT_MAIN_PAGE_DOWN: rep_ev(ses, doc_view, page_down); break;
		case ACT_MAIN_PAGE_UP: rep_ev(ses, doc_view, page_up); break;
		case ACT_MAIN_DOWN: rep_ev(ses, doc_view, down); break;
		case ACT_MAIN_UP: rep_ev(ses, doc_view, up); break;
		case ACT_MAIN_HOME: rep_ev(ses, doc_view, home); break;
		case ACT_MAIN_END: rep_ev(ses, doc_view, x_end); break;

		case ACT_MAIN_SCROLL_DOWN: scroll_down(ses, doc_view); break;
		case ACT_MAIN_SCROLL_UP: scroll_up(ses, doc_view); break;
		case ACT_MAIN_SCROLL_LEFT: rep_ev(ses, doc_view, scroll_left); break;
		case ACT_MAIN_SCROLL_RIGHT: rep_ev(ses, doc_view, scroll_right); break;

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

		case ACT_MAIN_ENTER:
			if (try_jump_to_link_number(ses, doc_view))
				status = FRAME_EVENT_OK;
			else
				status = enter(ses, doc_view, 0);
			break;
		case ACT_MAIN_ENTER_RELOAD:
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
		case ACT_MAIN_DOWNLOAD:
		case ACT_MAIN_RESUME_DOWNLOAD:
		case ACT_MAIN_VIEW_IMAGE:
		case ACT_MAIN_DOWNLOAD_IMAGE:
		case ACT_MAIN_LINK_MENU:
		case ACT_MAIN_OPEN_LINK_IN_NEW_WINDOW:
		case ACT_MAIN_OPEN_LINK_IN_NEW_TAB:
		case ACT_MAIN_OPEN_LINK_IN_NEW_TAB_IN_BACKGROUND:
			if (try_jump_to_link_number(ses, doc_view))
				status = FRAME_EVENT_OK;
			/* fall through */
		default:
			if (ev->x >= '1' && ev->x <= '9' && !ev->y) {
				/* FIXME: This probably doesn't work
				 * together with the keybinding...? */

				struct document *document = doc_view->document;
				int nlinks = document->nlinks, length;
				unsigned char d[2] = { ev->x, 0 };

				if (!nlinks) break;

				for (length = 1; nlinks; nlinks /= 10)
					length++;

				input_field(ses->tab->term, NULL, 1,
					    N_("Go to link"), N_("Enter link number"),
					    N_("OK"), N_("Cancel"), ses, NULL,
					    length, d, 1, document->nlinks, check_number,
					    (void (*)(void *, unsigned char *)) goto_link_number, NULL);

			} else if (get_opt_int("document.browse.accesskey.priority") == 1
				   && try_document_key(ses, doc_view, ev)) {
				/* The document ate the key! */
				status = FRAME_EVENT_OK;

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
	struct link *link = choose_mouse_link(doc_view, ev);

	if (check_mouse_wheel(ev)) {
		if (!check_mouse_action(ev, B_DOWN)) {
			/* We handle only B_DOWN case... */
		} else if (check_mouse_button(ev, B_WHEEL_UP)) {
			rep_ev(ses, doc_view, scroll_mouse_up);
		} else if (check_mouse_button(ev, B_WHEEL_DOWN)) {
			rep_ev(ses, doc_view, scroll_mouse_down);
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
			rep_ev(ses, doc_view, scroll_mouse_up);
		}
		if (ev->y >= doc_view->box.height - scrollmargin) {
			rep_ev(ses, doc_view, scroll_mouse_down);
		}

		if (ev->x < scrollmargin * 2) {
			rep_ev(ses, doc_view, scroll_mouse_left);
		}
		if (ev->x >= doc_view->box.width - scrollmargin * 2) {
			rep_ev(ses, doc_view, scroll_mouse_right);
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
	    status = field_op(ses, doc_view, link, ev, 0);

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

					} else goto x;
					ev->y |= ~KBD_ALT;
				}
		}

		if (doc_view
		    && get_opt_int("document.browse.accesskey.priority") <= 0
		    && try_document_key(ses, doc_view, ev)) {
			/* The document ate the key! */
			refresh_view(ses, doc_view, 0);
			return;
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
			int nb_tabs = number_of_tabs(ses->tab->term);
			int tab = get_tab_number_by_xpos(ses->tab->term, ev->x);

			if (check_mouse_button(ev, B_WHEEL_UP)) {
				switch_to_prev_tab(ses->tab->term);

			} else if (check_mouse_button(ev, B_WHEEL_DOWN)) {
				switch_to_next_tab(ses->tab->term);

			} else if (tab != -1) {
				switch_to_tab(ses->tab->term, tab, nb_tabs);

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
		case ACT_MAIN_RESUME_DOWNLOAD:
			download = resume_download;
		case ACT_MAIN_DOWNLOAD:
			ses->download_uri = get_link_uri(ses, doc_view, link);
			break;

		case ACT_MAIN_DOWNLOAD_IMAGE:
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
view_image(struct session *ses, struct document_view *doc_view, int a)
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
save_formatted_dlg(struct session *ses, struct document_view *doc_view, int a)
{
	query_file(ses, doc_view->vs->uri, ses, save_formatted, NULL, 1);
}

void
refresh_view(struct session *ses, struct document_view *doc_view, int frames)
{
	draw_doc(ses, doc_view, 1);
	if (frames) draw_frames(ses);
	print_screen_status(ses);
	redraw_from_window(ses->tab);
}
