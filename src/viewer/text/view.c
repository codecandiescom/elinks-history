/* HTML viewer (and much more) */
/* $Id: view.c,v 1.402 2004/04/23 20:44:30 pasky Exp $ */

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
find_tag(struct document *document, unsigned char *name)
{
	struct tag *tag;

	foreach (tag, document->tags)
		if (!strcasecmp(tag->name, name))
			return tag->y;

	return -1;
}

static void
draw_frame_lines(struct terminal *t, struct frameset_desc *frameset_desc,
		 int xp, int yp)
{
	/* Optionalize? */
	struct color_pair colors = INIT_COLOR_PAIR(0x000000, 0xCCCCCC);
	register int y, j;

	assert(t && frameset_desc && frameset_desc->frame_desc);
	if_assert_failed return;

	y = yp - 1;
	for (j = 0; j < frameset_desc->height; j++) {
		register int x, i;
		int height = frameset_desc->frame_desc[j * frameset_desc->width].height;

		x = xp - 1;
		for (i = 0; i < frameset_desc->width; i++) {
			int width = frameset_desc->frame_desc[i].width;

			if (i) {
				draw_area(t, x, y + 1, 1, height, BORDER_SVLINE,
					  SCREEN_ATTR_FRAME, &colors);
				if (j == frameset_desc->height - 1)
					draw_border_cross(t, x, y + height + 1,
							  BORDER_X_UP, &colors);
			} else if (j) {
				if (x >= 0)
					draw_border_cross(t, x, y,
							  BORDER_X_RIGHT, &colors);
			}

			if (j) {
				draw_area(t, x + 1, y, width, 1, BORDER_SHLINE,
					  SCREEN_ATTR_FRAME, &colors);
				if (i == frameset_desc->width - 1
				    && x + width + 1 < t->width)
					draw_border_cross(t, x + width + 1, y,
							  BORDER_X_LEFT, &colors);
			} else if (i) {
				draw_border_cross(t, x, y, BORDER_X_DOWN, &colors);
			}

			if (i && j)
				draw_border_char(t, x, y, BORDER_SCROSS, &colors);

			x += width + 1;
		}
		y += height + 1;
	}

	y = yp - 1;
	for (j = 0; j < frameset_desc->height; j++) {
		register int x, i;
		int pj = j * frameset_desc->width;
		int height = frameset_desc->frame_desc[pj].height;

		x = xp - 1;
		for (i = 0; i < frameset_desc->width; i++) {
			int width = frameset_desc->frame_desc[i].width;
			int p = pj + i;

			if (frameset_desc->frame_desc[p].subframe) {
				draw_frame_lines(t, frameset_desc->frame_desc[p].subframe,
						 x + 1, y + 1);
			}
			x += width + 1;
		}
		y += height + 1;
	}
}

void
draw_doc(struct terminal *t, struct document_view *doc_view, int active)
{
	struct color_pair color = INIT_COLOR_PAIR(0, 0);
	struct view_state *vs;
	int xp, yp;
	int width, height;
	int vx, vy;
	int y;

	assert(t && doc_view);
	if_assert_failed return;

	xp = doc_view->x;
	yp = doc_view->y;
	width = doc_view->width;
	height = doc_view->height;

	/* The code in this function assumes that both width and height are
	 * bigger than 1 so we have to bail out here. */
	if (width < 2 || height < 2) return;

	if (active) {
		set_cursor(t, xp + width - 1, yp + height - 1, 1);
		set_window_ptr(get_current_tab(t), xp, yp);
	}

	if (doc_view->document->height)
		color.background = doc_view->document->bgcolor;

	if (!doc_view->vs) {
		draw_area(t, xp, yp, width, height, ' ', 0, &color);
		return;
	}

	if (document_has_frames(doc_view->document)) {
	 	draw_area(t, xp, yp, width, height, ' ', 0, &color);
		draw_frame_lines(t, doc_view->document->frame_desc, xp, yp);
		if (doc_view->vs && doc_view->vs->current_link == -1)
			doc_view->vs->current_link = 0;
		return;
	}
	check_vs(doc_view);
	vs = doc_view->vs;
	if (vs->goto_position) {
		vy = find_tag(doc_view->document, vs->goto_position);
		if (vy != -1) {
			int_bounds(&vy, 0, doc_view->document->height - 1);
			vs->y = vy;
			set_link(doc_view);
			mem_free(vs->goto_position);
			vs->goto_position = NULL;
		}
	}
	vx = vs->x;
	vy = vs->y;
	if (doc_view->last_x != -1
	    && doc_view->last_x == vx
	    && doc_view->last_y == vy
	    && !has_search_word(doc_view)) {
		clear_link(t, doc_view);
		draw_forms(t, doc_view);
		if (active) draw_current_link(t, doc_view);
		return;
	}
	free_link(doc_view);
	doc_view->last_x = vx;
	doc_view->last_y = vy;
	draw_area(t, xp, yp, width, height, ' ', 0, &color);
	if (!doc_view->document->height) return;

	while (vs->y >= doc_view->document->height) vs->y -= height;
	int_lower_bound(&vs->y, 0);
	if (vy != vs->y) vy = vs->y, check_vs(doc_view);
	for (y = int_max(vy, 0);
	     y < int_min(doc_view->document->height, height + vy);
	     y++) {
		int st = int_max(vx, 0);
		int en = int_min(doc_view->document->data[y].length,
				 width + vx);

		if (en - st <= 0) continue;
		draw_line(t, xp + st - vx, yp + y - vy, en - st,
			  &doc_view->document->data[y].chars[st]);
	}
	draw_forms(t, doc_view);
	if (active) draw_current_link(t, doc_view);
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
				draw_doc(ses->tab->term, doc_view, doc_view == current_doc_view);
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
		draw_area(ses->tab->term, 0, 1, ses->tab->term->width,
			  ses->tab->term->height - 2, ' ', 0, NULL);
		return;
	}

	if (!ses->doc_view->vs && have_location(ses))
		ses->doc_view->vs = &cur_loc(ses)->vs;
	ses->doc_view->last_x = ses->doc_view->last_y = -1;
	draw_doc(ses->tab->term, ses->doc_view, 1);
	draw_frames(ses);
	print_screen_status(ses);
	redraw_from_window(ses->tab);
}

static void
page_down(struct session *ses, struct document_view *doc_view, int a)
{
	int newpos;

	assert(ses && doc_view && doc_view->vs);
	if_assert_failed return;

	newpos = doc_view->vs->y + doc_view->height;
	if (newpos < doc_view->document->height)
		doc_view->vs->y = newpos;

	if (!current_link_is_visible(doc_view))
		find_link(doc_view, 1, a);
}

static void
page_up(struct session *ses, struct document_view *doc_view, int a)
{
	assert(ses && doc_view && doc_view->vs);
	if_assert_failed return;

	if (doc_view->vs->y == 0) return;
	doc_view->vs->y -= doc_view->height;
	int_lower_bound(&doc_view->vs->y, 0);

	if (!current_link_is_visible(doc_view))
		find_link(doc_view, -1, a);
}


void
down(struct session *ses, struct document_view *doc_view, int a)
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
		page_down(ses, doc_view, 1);
	}

	if (current_link != doc_view->vs->current_link) {
		set_textarea(ses, doc_view, KBD_UP);
	}
}

static void
up(struct session *ses, struct document_view *doc_view, int a)
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
		page_up(ses, doc_view, 1);
	}

	if (current_link != doc_view->vs->current_link) {
		set_textarea(ses, doc_view, KBD_DOWN);
	}
}


/* Fix namespace clash on MacOS. */
#define scrool scroll_elinks

void
scroll(struct session *ses, struct document_view *doc_view, int a)
{
	int max_height;

	assert(ses && doc_view && doc_view->vs && doc_view->document);
	if_assert_failed return;

	max_height = doc_view->document->height - doc_view->height;
	if (a > 0 && doc_view->vs->y >= max_height) return;
	doc_view->vs->y += a;
	if (a > 0) int_upper_bound(&doc_view->vs->y, max_height);
	int_lower_bound(&doc_view->vs->y, 0);

	if (!current_link_is_visible(doc_view))
		find_link(doc_view, a < 0 ? -1 : 1, 0);
}

static void
hscroll(struct session *ses, struct document_view *doc_view, int a)
{
	int x;

	assert(ses && doc_view && doc_view->vs && doc_view->document);
	if_assert_failed return;

	x = doc_view->vs->x + a;
	int_bounds(&x, 0, doc_view->document->width - 1);
	if (x == doc_view->vs->x) return;

	doc_view->vs->x = x;

	if (!current_link_is_visible(doc_view))
		find_link(doc_view, 1, 0);
	/* !!! FIXME: check right margin */
}

static void
home(struct session *ses, struct document_view *doc_view, int a)
{
	assert(ses && doc_view && doc_view->vs);
	if_assert_failed return;

	doc_view->vs->y = doc_view->vs->x = 0;
	find_link(doc_view, 1, 0);
}

static void
x_end(struct session *ses, struct document_view *doc_view, int a)
{
	int max_height;

	assert(ses && doc_view && doc_view->vs && doc_view->document);
	if_assert_failed return;

	max_height = doc_view->document->height - doc_view->height;
	doc_view->vs->x = 0;
	int_lower_bound(&doc_view->vs->y, int_max(0, max_height));
	find_link(doc_view, -1, 0);
}

void
set_frame(struct session *ses, struct document_view *doc_view, int a)
{
	assert(ses && ses->doc_view && doc_view && doc_view->vs);
	if_assert_failed return;

	if (doc_view == ses->doc_view) return;
	goto_url(ses, struri(doc_view->vs->uri));
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
       void (*f)(struct session *, struct document_view *, int),
       int a)
{
	register int i;

	assert(ses && doc_view && f);
	if_assert_failed return;

	i = ses->kbdprefix.rep ? ses->kbdprefix.rep_num : 1;
	while (i--) f(ses, doc_view, a);
}


/* We return |x| at the end of the function. The value of x
 * should be one of the following:
 *
 * value  signifies
 * 0      the event was not handled
 * 1      the event was handled, and the screen should be redrawn
 * 2      the event was handled, and the screen should _not_ be redrawn
 */
static int
frame_ev(struct session *ses, struct document_view *doc_view, struct term_event *ev)
{
	struct link *link;
	int x = 1;

	assert(ses && doc_view && doc_view->document && doc_view->vs && ev);
	if_assert_failed return 1;

	link = get_current_link(doc_view);

	if (link
	    && link_is_textinput(link)
	    && field_op(ses, doc_view, link, ev, 0))
		return 1;

	if (ev->ev == EV_KBD) {
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
				{
					struct view_state *vs;

					vs = get_mark(mark);
					if (!vs) break;

					/* TODO: Support for cross-document
					 * marks. See marks.c for detailed
					 * TODOs. --pasky */
					if (doc_view->vs->uri != vs->uri)
						break;

					destroy_vs(doc_view->vs);
					copy_vs(doc_view->vs, vs);
					break;
				}
			}

			ses->kbdprefix.rep = 0;
			ses->kbdprefix.mark = KP_MARK_NOTHING;
			return 1;
		}

		if (ev->x >= '0' + !ses->kbdprefix.rep && ev->x <= '9'
		    && (ev->y
			|| !doc_view->document->options.num_links_key
			|| (doc_view->document->options.num_links_key == 1
			    && !doc_view->document->options.num_links_display))) {
			/* Repeat count */

			if (!ses->kbdprefix.rep) {
				ses->kbdprefix.rep_num = ev->x - '0';
			} else {
				ses->kbdprefix.rep_num = ses->kbdprefix.rep_num * 10
							 + ev->x - '0';
			}

			int_upper_bound(&ses->kbdprefix.rep_num, 65536);

			ses->kbdprefix.rep = 1;
			return 2;
		}

		if (get_opt_int("document.browse.accesskey.priority") >= 2
		    && try_document_key(ses, doc_view, ev)) {
			/* The document ate the key! */
			return 1;
		}

		switch (kbd_action(KM_MAIN, ev, NULL)) {
			case ACT_MAIN_COPY_CLIPBOARD:
			case ACT_MAIN_ENTER:
			case ACT_MAIN_ENTER_RELOAD:
			case ACT_MAIN_DOWNLOAD:
			case ACT_MAIN_RESUME_DOWNLOAD:
			case ACT_MAIN_VIEW_IMAGE:
			case ACT_MAIN_DOWNLOAD_IMAGE:
			case ACT_MAIN_LINK_MENU:
			case ACT_MAIN_JUMP_TO_LINK:
			case ACT_MAIN_OPEN_LINK_IN_NEW_WINDOW:
			case ACT_MAIN_OPEN_LINK_IN_NEW_TAB:
			case ACT_MAIN_OPEN_LINK_IN_NEW_TAB_IN_BACKGROUND:
				if (!ses->kbdprefix.rep) break;

				if (ses->kbdprefix.rep_num
				    > doc_view->document->nlinks) {
					ses->kbdprefix.rep = 0;
					return 2;
				}

				jump_to_link_number(ses,
						    current_frame(ses),
						    ses->kbdprefix.rep_num
							- 1);

				draw_doc(ses->tab->term, doc_view, 1);
				print_screen_status(ses);
				redraw_from_window(ses->tab);
		}

		switch (kbd_action(KM_MAIN, ev, NULL)) {
			case ACT_MAIN_PAGE_DOWN: rep_ev(ses, doc_view, page_down, 0); break;
			case ACT_MAIN_PAGE_UP: rep_ev(ses, doc_view, page_up, 0); break;
			case ACT_MAIN_DOWN: rep_ev(ses, doc_view, down, 0); break;
			case ACT_MAIN_UP: rep_ev(ses, doc_view, up, 0); break;
			case ACT_MAIN_COPY_CLIPBOARD: {
				char *current_link = print_current_link(ses);

				if (current_link) {
					set_clipboard_text(current_link);
					mem_free(current_link);
				}
				break;
			}

			/* XXX: Code duplication of following for mouse */
			case ACT_MAIN_SCROLL_UP: scroll(ses, doc_view, ses->kbdprefix.rep ? -ses->kbdprefix.rep_num : -get_opt_int("document.browse.scroll_step")); break;
			case ACT_MAIN_SCROLL_DOWN: scroll(ses, doc_view, ses->kbdprefix.rep ? ses->kbdprefix.rep_num : get_opt_int("document.browse.scroll_step")); break;
			case ACT_MAIN_SCROLL_LEFT: rep_ev(ses, doc_view, hscroll, -1 - 7 * !ses->kbdprefix.rep); break;
			case ACT_MAIN_SCROLL_RIGHT: rep_ev(ses, doc_view, hscroll, 1 + 7 * !ses->kbdprefix.rep); break;

			case ACT_MAIN_HOME: rep_ev(ses, doc_view, home, 0); break;
			case ACT_MAIN_END:  rep_ev(ses, doc_view, x_end, 0); break;
			case ACT_MAIN_ENTER: x = enter(ses, doc_view, 0); break;
			case ACT_MAIN_ENTER_RELOAD: x = enter(ses, doc_view, 1); break;
			case ACT_MAIN_JUMP_TO_LINK: x = 2; break;
			case ACT_MAIN_MARK_SET:
				ses->kbdprefix.mark = KP_MARK_SET;
				x = 2;
				break;
			case ACT_MAIN_MARK_GOTO:
				/* TODO: Show promptly a menu (or even listbox?)
				 * with all the marks. But the next letter must
				 * still choose a mark directly! --pasky */
				ses->kbdprefix.mark = KP_MARK_GOTO;
				x = 2;
				break;
			default:
				if (ev->x >= '1' && ev->x <= '9' && !ev->y) {
					/* FIXME: This probably doesn't work
					 * together with the keybinding...? */

					struct document *document = doc_view->document;
					int nl, lnl;
					unsigned char d[2];

					d[0] = ev->x;
					d[1] = 0;
					nl = document->nlinks, lnl = 1;
					while (nl) nl /= 10, lnl++;
					if (lnl > 1)
						input_field(ses->tab->term, NULL, 1,
							    N_("Go to link"), N_("Enter link number"),
							    N_("OK"), N_("Cancel"), ses, NULL,
							    lnl, d, 1, document->nlinks, check_number,
							    (void (*)(void *, unsigned char *)) goto_link_number, NULL);
				} else if (get_opt_int("document.browse.accesskey.priority") == 1
					   && try_document_key(ses, doc_view, ev)) {
					/* The document ate the key! */
					return 1;

				} else {
					x = 0;
				}
		}
#ifdef CONFIG_MOUSE
	} else if (ev->ev == EV_MOUSE) {
		/* TODO: Pop up the tab_menu() when right clicking is not
		 * handled by anything else here. */
		struct link *link = choose_mouse_link(doc_view, ev);

		if (check_mouse_wheel(ev)) {
			if (!check_mouse_action(ev, B_DOWN)) {
				/* We handle only B_DOWN case... */
			} else if (check_mouse_button(ev, B_WHEEL_UP)) {
				rep_ev(ses, doc_view, scroll, -2);
			} else if (check_mouse_button(ev, B_WHEEL_DOWN)) {
				rep_ev(ses, doc_view, scroll, 2);
			}

		} else if (link) {
			x = 1;
			doc_view->vs->current_link = link - doc_view->document->links;

			if (!link_is_textinput(link)
			    && check_mouse_action(ev, B_UP)) {

				draw_doc(ses->tab->term, doc_view, 1);
				print_screen_status(ses);
				redraw_from_window(ses->tab);

				if (check_mouse_button(ev, B_LEFT))
					x = enter(ses, doc_view, 0);
				else if (check_mouse_button(ev, B_MIDDLE))
					open_current_link_in_new_tab(ses, 1);
				else
					link_menu(ses->tab->term, NULL, ses);
			}
		} else {
			/* Clicking to the edge of screen (TODO: possibly only
			 * with certain button?) will make the document scroll
			 * automagically. */

			int scrollmargin = get_opt_int("document.browse.scroll_margin");

			/* XXX: This is code duplication with kbd handlers. But
			 * repeatcount-free here. */

			if (ev->y < scrollmargin) {
				rep_ev(ses, doc_view, scroll, -2);
			}
			if (ev->y >= doc_view->height - scrollmargin) {
				rep_ev(ses, doc_view, scroll, 2);
			}

			if (ev->x < scrollmargin * 2) {
				rep_ev(ses, doc_view, hscroll, -8);
			}
			if (ev->x >= doc_view->width - scrollmargin * 2) {
				rep_ev(ses, doc_view, hscroll, 8);
			}
		}
#endif /* CONFIG_MOUSE */
	} else {
		x = 0;
	}

	ses->kbdprefix.rep = 0;
	return x;
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

static int
send_to_frame(struct session *ses, struct term_event *ev)
{
	struct document_view *doc_view;
	int r;

	assert(ses && ses->tab && ses->tab->term && ev);
	if_assert_failed return 0;
	doc_view = current_frame(ses);
	assertm(doc_view, "document not formatted");
	if_assert_failed return 0;

	r = frame_ev(ses, doc_view, ev);
	if (r == 1) {
		draw_doc(ses->tab->term, doc_view, 1);
		print_screen_status(ses);
		redraw_from_window(ses->tab);
	}

	return r;
}

#ifdef CONFIG_MOUSE
static void
do_mouse_event(struct session *ses, struct term_event *ev,
	       struct document_view *doc_view)
{
	struct term_event evv;
	struct document_view *matched = NULL, *first = doc_view;

	assert(ses && ev);
	if_assert_failed return;

	do {
		struct document_options *o = &doc_view->document->options;

		assert(doc_view && doc_view->document);
		if_assert_failed return;

		if (ev->x >= o->x && ev->x < o->x + doc_view->width
		    && ev->y >= o->y && ev->y < o->y + doc_view->height) {
			matched = doc_view;
			break;
		}

		next_frame(ses, 1);
		doc_view = current_frame(ses);

	} while (doc_view != first);

	if (!matched) return;

	if (doc_view != first) draw_formatted(ses, 0);

	memcpy(&evv, ev, sizeof(struct term_event));
	evv.x -= doc_view->x;
	evv.y -= doc_view->y;
	send_to_frame(ses, &evv);
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

		if (doc_view && send_to_frame(ses, ev)) return;

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
#ifdef HAVE_SCRIPTING
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
			draw_doc(ses->tab->term, doc_view, 1);
			print_screen_status(ses);
			redraw_from_window(ses->tab);
			return;
		}
	}
#ifdef CONFIG_MOUSE
	if (ev->ev == EV_MOUSE) {
		int bars;

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
		if (doc_view) do_mouse_event(ses, ev, doc_view);
	}
#endif /* CONFIG_MOUSE */
	return;

x:
	/* ses may disappear ie. in close_tab() */
	if (ses) ses->kbdprefix.rep = 0;
}

void
send_enter(struct terminal *term, void *xxx, struct session *ses)
{
	struct term_event ev = INIT_TERM_EVENT(EV_KBD, KBD_ENTER, 0, 0);

	assert(ses);
	if_assert_failed return;
	send_event(ses, &ev);
}

void
download_link(struct session *ses, struct document_view *doc_view, int action)
{
	struct link *link = get_current_link(doc_view);
	void (*download)(void *ses, unsigned char *file) = start_download;
	unsigned char *url;

	if (!link) return;

	if (ses->download_uri) {
		done_uri(ses->download_uri);
		ses->download_uri = NULL;
	}

	switch (action) {
		case ACT_MAIN_RESUME_DOWNLOAD:
			download = resume_download;
		case ACT_MAIN_DOWNLOAD:
			url = get_link_url(ses, doc_view, link);
			break;

		case ACT_MAIN_DOWNLOAD_IMAGE:
			url = stracpy(link->where_img);
			break;

		default:
			INTERNAL("I think you forgot to take your medication, mental boy!");
			return;
	}

	if (!url || !strncasecmp(url, "MAP@", 4)) {
		mem_free_if(url);
		return;
	}

	ses->download_uri = get_uri(url, -1);
	mem_free(url);
	if (!ses->download_uri) return;

	set_session_referrer(ses, doc_view->document->uri);
	query_file(ses, ses->download_uri, ses, download, NULL, 1);
}

void
save_url(struct session *ses, unsigned char *url)
{
	struct document_view *doc_view;
	struct uri *uri;

	assert(ses && ses->tab && ses->tab->term && url);
	if_assert_failed return;

	if (!*url) return;

	uri = get_translated_uri(url, ses->tab->term->cwd, NULL);
	if (!uri) {
		print_error_dialog(ses, S_BAD_URL, PRI_CANCEL);
		return;
	}

	if (ses->download_uri) done_uri(ses->download_uri);
	ses->download_uri = uri;

	doc_view = current_frame(ses);
	assert(doc_view && doc_view->document && doc_view->document->uri);
	if_assert_failed return;

	set_session_referrer(ses, doc_view->document->uri);
	query_file(ses, ses->download_uri, ses, start_download, NULL, 1);
}

void
view_image(struct session *ses, struct document_view *doc_view, int a)
{
	struct link *link = get_current_link(doc_view);

	if (link && link->where_img)
		goto_url(ses, link->where_img);
}

void
save_as(struct terminal *term, void *xxx, struct session *ses)
{
	struct document_view *doc_view = current_frame(ses);
	struct location *loc;

	assert(term && ses);
	if_assert_failed return;

	if (!have_location(ses)) return;

	loc = cur_loc(ses);

	if (ses->download_uri) done_uri(ses->download_uri);
	ses->download_uri = get_uri_reference(loc->vs.uri);

	doc_view = current_frame(ses);

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
