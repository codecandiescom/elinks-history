/* HTML viewer (and much more) */
/* $Id: view.c,v 1.244 2003/10/30 13:12:59 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "elinks.h"

#include "main.h"
#include "bfu/inpfield.h"
#include "bfu/menu.h"
#include "bfu/msgbox.h"
#include "dialogs/menu.h"
#include "bookmarks/dialogs.h"
#include "cookies/cookies.h"
#include "config/dialogs.h"
#include "config/kbdbind.h"
#include "config/options.h"
#include "dialogs/document.h"
#include "document/cache.h"
#include "document/document.h"
#include "document/options.h"
#include "document/html/renderer.h"
#include "globhist/dialogs.h"
#include "intl/charsets.h"
#include "intl/gettext/libintl.h"
#include "osdep/newwin.h"
#include "osdep/osdep.h"
#include "protocol/http/auth.h"
#include "protocol/uri.h"
#include "sched/download.h"
#include "sched/event.h"
#include "sched/location.h"
#include "sched/session.h"
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
		format_cache_reactivate(doc_view->document);
		if (!--doc_view->document->refcount) {
			format_cache_entries++;
		}
		assertm(doc_view->document->refcount >= 0,
			"format_cache refcount underflow");
		if_assert_failed doc_view->document->refcount = 0;
		doc_view->document = NULL;
	}
	doc_view->vs = NULL;
	if (doc_view->link_bg) {
		mem_free(doc_view->link_bg), doc_view->link_bg = NULL;
		doc_view->link_bg_n = 0;
	}
	if (doc_view->name) {
		mem_free(doc_view->name), doc_view->name = NULL;
	}
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
				    && x + width + 1 < t->x)
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
			if (vy > doc_view->document->height)
				vy = doc_view->document->height - 1; /* XXX:zas: -1 ?? */
			if (vy < 0) vy = 0;
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
	if (vs->y < 0) vs->y = 0;
	if (vy != vs->y) vy = vs->y, check_vs(doc_view);
	for (y = int_max(vy, 0);
	     y < int_min(doc_view->document->height, height + vy);
	     y++) {
		int st = int_max(vx, 0);
		int en = int_min(doc_view->document->data[y].l, width + vx);

		if (en - st <= 0) continue;
		draw_line(t, xp + st - vx, yp + y - vy, en - st,
			  &doc_view->document->data[y].d[st]);
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
	int n, i, d, more;

	assert(ses && ses->doc_view && ses->doc_view->document);
	if_assert_failed return;

	if (!document_has_frames(ses->doc_view->document)) return;
	n = 0;
	foreach (doc_view, ses->scrn_frames) {
	       doc_view->last_x = doc_view->last_y = -1;
	       n++;
	}
	l = &cur_loc(ses)->vs.current_link;
	if (*l < 0) *l = 0;
	if (!n) n = 1;
	*l %= n;
	i = *l;
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
draw_formatted(struct session *ses)
{
	assert(ses && ses->tab);
	if_assert_failed return;

	if (ses->tab != get_current_tab(ses->tab->term))
		return;

	if (!ses->doc_view || !ses->doc_view->document) {
		/*internal("document not formatted");*/
		draw_area(ses->tab->term, 0, 1, ses->tab->term->x,
			  ses->tab->term->y - 2, ' ', 0, NULL);
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

	newpos = doc_view->vs->y + doc_view->document->options.yw;
	if (newpos < doc_view->document->height) {
		doc_view->vs->y = newpos;
		find_link(doc_view, 1, a);
	} else {
		find_link(doc_view, -1, a);
	}
}

static void
page_up(struct session *ses, struct document_view *doc_view, int a)
{
	assert(ses && doc_view && doc_view->vs);
	if_assert_failed return;

	doc_view->vs->y -= doc_view->height;
	find_link(doc_view, -1, a);
	if (doc_view->vs->y < 0) doc_view->vs->y = 0/*, find_link(f, 1, a)*/;
}


void
down(struct session *ses, struct document_view *doc_view, int a)
{
	int current_link;

	assert(ses && doc_view && doc_view->vs && doc_view->document);
	if_assert_failed return;

	current_link = doc_view->vs->current_link;

	if (get_opt_int("document.browse.links.wraparound")
	    && current_link >= doc_view->document->nlinks - 1) {
		jump_to_link_number(ses, doc_view, 0);
		/* FIXME: This needs further work, we should call page_down()
		 * and set_textarea() under some conditions as well. --pasky */
		return;
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

	if (get_opt_int("document.browse.links.wraparound")
	    && current_link == 0) {
		jump_to_link_number(ses, doc_view, doc_view->document->nlinks - 1);
		/* FIXME: This needs further work, we should call page_down()
		 * and set_textarea() under some conditions as well. --pasky */
		return;
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

static void
scroll(struct session *ses, struct document_view *doc_view, int a)
{
	assert(ses && doc_view && doc_view->vs && doc_view->document);
	if_assert_failed return;

	/* XXX:zas: use intermediate variable here. */
	if (a > 0 && doc_view->vs->y >= doc_view->document->height - doc_view->document->options.yw)
		return;
	doc_view->vs->y += a;
	if (a > 0)
		int_upper_bound(&doc_view->vs->y, doc_view->document->height - doc_view->document->options.yw);
	int_lower_bound(&doc_view->vs->y, 0);
	if (c_in_view(doc_view)) return;
	find_link(doc_view, a < 0 ? -1 : 1, 0);
}

static void
hscroll(struct session *ses, struct document_view *doc_view, int a)
{
	assert(ses && doc_view && doc_view->vs && doc_view->document);
	if_assert_failed return;

	doc_view->vs->x += a;
	int_bounds(&doc_view->vs->x, 0, doc_view->document->width - 1);

	if (c_in_view(doc_view)) return;
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
	assert(ses && doc_view && doc_view->vs && doc_view->document);
	if_assert_failed return;

	doc_view->vs->x = 0;
	int_lower_bound(&doc_view->vs->y, doc_view->document->height - doc_view->document->options.yw);
	int_lower_bound(&doc_view->vs->y, 0);
	find_link(doc_view, -1, 0);
}

void
set_frame(struct session *ses, struct document_view *doc_view, int a)
{
	assert(ses && ses->doc_view && doc_view && doc_view->vs);
	if_assert_failed return;

	if (doc_view == ses->doc_view) return;
	goto_url(ses, doc_view->vs->url);
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
	html_interpret(ses);
	draw_formatted(ses);
}

void
toggle_images(struct session *ses, struct document_view *doc_view, int a)
{
	assert(ses && doc_view && ses->tab && ses->tab->term);
	if_assert_failed return;

	if (!doc_view->vs) {
		nowhere_box(ses->tab->term, NULL);
		return;
	}

	/* TODO: toggle per document. --Zas */
	get_opt_int("document.browse.images.show_as_links") =
		!get_opt_int("document.browse.images.show_as_links");

	html_interpret(ses);
	draw_formatted(ses);
}

void
toggle_link_numbering(struct session *ses, struct document_view *doc_view, int a)
{
	assert(ses && doc_view && ses->tab && ses->tab->term);
	if_assert_failed return;

	if (!doc_view->vs) {
		nowhere_box(ses->tab->term, NULL);
		return;
	}

	/* TODO: toggle per document. --Zas */
	get_opt_int("document.browse.links.numbering") =
		!get_opt_int("document.browse.links.numbering");

	html_interpret(ses);
	draw_formatted(ses);
}

void
toggle_document_colors(struct session *ses, struct document_view *doc_view, int a)
{
	assert(ses && doc_view && ses->tab && ses->tab->term);
	if_assert_failed return;

	if (!doc_view->vs) {
		nowhere_box(ses->tab->term, NULL);
		return;
	}

	/* TODO: toggle per document. --Zas */
	{
		int mode = get_opt_int("document.colors.use_document_colors");

		get_opt_int("document.colors.use_document_colors") =
			(mode + 1 <= 2) ? mode + 1 : 0;
	}

	html_interpret(ses);
	draw_formatted(ses);
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


static void
frm_download(struct session *ses, struct document_view *doc_view, int resume)
{
	struct link *link;

	assert(ses && doc_view && doc_view->vs && doc_view->document);
	if_assert_failed return;

	if (doc_view->vs->current_link == -1) return;
	if (ses->dn_url) {
		mem_free(ses->dn_url);
		ses->dn_url = NULL;
	}
	link = &doc_view->document->links[doc_view->vs->current_link];
	if (link->type != L_LINK && link->type != L_BUTTON) return;

	ses->dn_url = get_link_url(ses, doc_view, link);
	if (ses->dn_url) {
		if (!strncasecmp(ses->dn_url, "MAP@", 4)) {
			mem_free(ses->dn_url);
			ses->dn_url = NULL;
			return;
		}
		if (ses->ref_url) mem_free(ses->ref_url);
		ses->ref_url = stracpy(doc_view->document->url);
		query_file(ses, ses->dn_url, (resume ? resume_download : start_download), NULL, 1);
	}
}


static int
frame_ev(struct session *ses, struct document_view *doc_view, struct term_event *ev)
{
	int x = 1;

	assert(ses && doc_view && doc_view->document && doc_view->vs && ev);
	if_assert_failed return 1;

	if (doc_view->vs->current_link >= 0
	    && (doc_view->document->links[doc_view->vs->current_link].type == L_FIELD ||
		doc_view->document->links[doc_view->vs->current_link].type == L_AREA)
	    && field_op(ses, doc_view, &doc_view->document->links[doc_view->vs->current_link], ev, 0))
		return 1;

	if (ev->ev == EV_KBD) {
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
			case ACT_COPY_CLIPBOARD:
			case ACT_ENTER:
			case ACT_ENTER_RELOAD:
			case ACT_DOWNLOAD:
			case ACT_RESUME_DOWNLOAD:
			case ACT_VIEW_IMAGE:
			case ACT_DOWNLOAD_IMAGE:
			case ACT_LINK_MENU:
			case ACT_JUMP_TO_LINK:
			case ACT_OPEN_LINK_IN_NEW_WINDOW:
			case ACT_OPEN_LINK_IN_NEW_TAB:
			case ACT_OPEN_LINK_IN_NEW_TAB_IN_BACKGROUND:
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
		}

		switch (kbd_action(KM_MAIN, ev, NULL)) {
			case ACT_PAGE_DOWN: rep_ev(ses, doc_view, page_down, 0); break;
			case ACT_PAGE_UP: rep_ev(ses, doc_view, page_up, 0); break;
			case ACT_DOWN: rep_ev(ses, doc_view, down, 0); break;
			case ACT_UP: rep_ev(ses, doc_view, up, 0); break;
			case ACT_COPY_CLIPBOARD: {
				char *current_link = print_current_link(ses);

				if (current_link) {
					set_clipboard_text(current_link);
					mem_free(current_link);
				}
				break;
			}

			/* XXX: Code duplication of following for mouse */
			case ACT_SCROLL_UP: scroll(ses, doc_view, ses->kbdprefix.rep ? -ses->kbdprefix.rep_num : -get_opt_int("document.browse.scroll_step")); break;
			case ACT_SCROLL_DOWN: scroll(ses, doc_view, ses->kbdprefix.rep ? ses->kbdprefix.rep_num : get_opt_int("document.browse.scroll_step")); break;
			case ACT_SCROLL_LEFT: rep_ev(ses, doc_view, hscroll, -1 - 7 * !ses->kbdprefix.rep); break;
			case ACT_SCROLL_RIGHT: rep_ev(ses, doc_view, hscroll, 1 + 7 * !ses->kbdprefix.rep); break;

			case ACT_HOME: rep_ev(ses, doc_view, home, 0); break;
			case ACT_END:  rep_ev(ses, doc_view, x_end, 0); break;
			case ACT_ENTER: x = enter(ses, doc_view, 0); if (x == 2 && ses->kbdprefix.rep) x = 1; break;
			case ACT_ENTER_RELOAD: x = enter(ses, doc_view, 1); if (x == 2 && ses->kbdprefix.rep) x = 1; break;
			case ACT_DOWNLOAD: if (!get_opt_int_tree(cmdline_options, "anonymous")) frm_download(ses, doc_view, 0); break;
			case ACT_RESUME_DOWNLOAD: if (!get_opt_int_tree(cmdline_options, "anonymous")) frm_download(ses, doc_view, 1); break;
			case ACT_SEARCH: search_dlg(ses, doc_view, 0); break;
			case ACT_SEARCH_BACK: search_back_dlg(ses, doc_view, 0); break;
			case ACT_FIND_NEXT: find_next(ses, doc_view, 0); break;
			case ACT_FIND_NEXT_BACK: find_next_back(ses, doc_view, 0); break;
			case ACT_ZOOM_FRAME: set_frame(ses, doc_view, 0), x = 2; break;
			case ACT_VIEW_IMAGE: send_image(ses->tab->term, NULL, ses); break;
			case ACT_DOWNLOAD_IMAGE: send_download_image(ses->tab->term, NULL, ses); break;
			case ACT_LINK_MENU: link_menu(ses->tab->term, NULL, ses); break;
			case ACT_JUMP_TO_LINK: break;
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
				}
#if 0
				else if (ev->x == 'x') {
					struct node *node;
					static int n = -1;
					int i;
					fd->xl = -1234;
					draw_doc(ses->tab->term, fd, 1);
					clear_link(ses->tab->term, fd);
					n++;
					i = n;
					foreachback (node, fd->document->nodes) {
						if (!i--) {
							int x, y;
							for (y = 0; y < node->yw; y++) for (x = 0; x < node->xw && x < 1000; x++) {
								int rx = x + node->x + fd->xp - fd->vs->x;
								int ry = y + node->y + fd->yp - fd->vs->y;
								if (rx >= 0 && ry >= 0 && rx < ses->tab->term->x && ry < ses->tab->term->y) {
									set_color(ses->tab->term, rx, ry, 0x3800);
								}
							}
							break;
						}
					}
					if (i >= 0) n = -1;
					x = 0;
				}
#endif
				else if (get_opt_int("document.browse.accesskey.priority") == 1
					 && try_document_key(ses, doc_view, ev)) {
					/* The document ate the key! */
					return 1;

				} else {
					x = 0;
				}
		}
#ifdef USE_MOUSE
	} else if (ev->ev == EV_MOUSE) {
		struct link *link = choose_mouse_link(doc_view, ev);

		if ((ev->b & BM_BUTT) >= B_WHEEL_UP) {
			if ((ev->b & BM_ACT) != B_DOWN) {
				/* We handle only B_DOWN case... */
			} else if ((ev->b & BM_BUTT) == B_WHEEL_UP) {
				rep_ev(ses, doc_view, scroll, -2);
			} else if ((ev->b & BM_BUTT) == B_WHEEL_DOWN) {
				rep_ev(ses, doc_view, scroll, 2);
			}

		} else if (link) {
			x = 1;
			doc_view->vs->current_link = link - doc_view->document->links;

			if ((link->type == L_LINK || link->type == L_BUTTON ||
			     link->type == L_CHECKBOX || link->type == L_SELECT)
			    && (ev->b & BM_ACT) == B_UP) {

				draw_doc(ses->tab->term, doc_view, 1);
				print_screen_status(ses);
				redraw_from_window(ses->tab);

				if ((ev->b & BM_BUTT) < B_MIDDLE)
					x = enter(ses, doc_view, 0);
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
#endif /* USE_MOUSE */
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
	int i;

	assert(ses);
	if_assert_failed return NULL;

	if (!have_location(ses)) return NULL;
	i = cur_loc(ses)->vs.current_link;
	foreach (doc_view, ses->scrn_frames) {
		if (document_has_frames(doc_view->document)) continue;
		if (!i--) return doc_view;
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

#ifdef USE_MOUSE
static void
do_mouse_event(struct session *ses, struct term_event *ev,
	       struct document_view *doc_view)
{
	struct term_event evv;
	struct document_view *current_doc_view; /* !!! FIXME: frames */
	struct document_options *o;

	assert(ses && ev && doc_view && doc_view->document);
	if_assert_failed return;

	o = &doc_view->document->options;
	if (ev->x >= o->xp && ev->x < o->xp + o->xw &&
	    ev->y >= o->yp && ev->y < o->yp + o->yw) goto ok;

r:
	next_frame(ses, 1);
	current_doc_view = current_frame(ses);
	assert(current_doc_view && current_doc_view->document);
	if_assert_failed return;
	o = &current_doc_view->document->options;
	if (ev->x >= o->xp && ev->x < o->xp + o->xw &&
	    ev->y >= o->yp && ev->y < o->yp + o->yw) {
		draw_formatted(ses);
		doc_view = current_doc_view;
		goto ok;
	}
	if (current_doc_view != doc_view) goto r;
	return;

ok:
	memcpy(&evv, ev, sizeof(struct term_event));
	evv.x -= doc_view->x;
	evv.y -= doc_view->y;
	send_to_frame(ses, &evv);
}
#endif /* USE_MOUSE */

void
send_event(struct session *ses, struct term_event *ev)
{
	struct document_view *doc_view;

	assert(ses && ev);
	if_assert_failed return;
	doc_view = current_frame(ses);

	if (ev->ev == EV_KBD) {
		int func_ref;

		if (doc_view && send_to_frame(ses, ev)) return;

		switch (kbd_action(KM_MAIN, ev, &func_ref)) {
			case ACT_MENU:
				activate_bfu_technology(ses, -1);
				goto x;
			case ACT_FILE_MENU:
				activate_bfu_technology(ses, 0);
				goto x;
			case ACT_NEXT_FRAME:
				next_frame(ses, 1);
				draw_formatted(ses);
				/*draw_frames(ses);
				  print_screen_status(ses);
				  redraw_from_window(ses->tab);*/
				goto x;
			case ACT_PREVIOUS_FRAME:
				next_frame(ses, -1);
				draw_formatted(ses);
				goto x;
			case ACT_BACK:
				go_back(ses);
				goto x;
			case ACT_UNBACK:
				go_unback(ses);
				goto x;
			case ACT_RELOAD:
				reload(ses, -1);
				goto x;
			case ACT_ABORT_CONNECTION:
				abort_loading(ses, 1);
				print_screen_status(ses);
				goto x;
			case ACT_GOTO_URL:
quak:
				dialog_goto_url(ses,"");
				goto x;
			case ACT_GOTO_URL_CURRENT: {
				unsigned char *s, *postchar;
				struct location *loc;

				if (!have_location(ses)) goto quak;

				loc = cur_loc(ses);
				s = memacpy(loc->vs.url, loc->vs.url_len);
				if (s) {
					postchar = strchr(s, POST_CHAR);
					if (postchar) *postchar = 0;
					dialog_goto_url(ses, s);
					mem_free(s);
				}
				goto x;
			}
			case ACT_GOTO_URL_CURRENT_LINK: {
				unsigned char url[MAX_STR_LEN];

				if (!get_current_link_url(ses, url, sizeof url)) goto quak;
				dialog_goto_url(ses, url);
				goto x;
			}
			case ACT_GOTO_URL_HOME: {
				unsigned char *url = getenv("WWW_HOME");

				if (!url || !*url) url = WWW_HOME_URL;
				goto_url_with_hook(ses, url);
				goto x;
			}
			case ACT_FORGET_CREDENTIALS:
				free_auth();
				shrink_memory(1); /* flush caches */
				goto x;
			case ACT_SAVE_FORMATTED:
				/* TODO: if (!anonymous) for non-HTTI ? --pasky */
				menu_save_formatted(ses->tab->term, (void *) 1, ses);
				goto x;
			case ACT_ADD_BOOKMARK:
#ifdef BOOKMARKS
				if (!get_opt_int_tree(cmdline_options, "anonymous"))
					launch_bm_add_doc_dialog(ses->tab->term, NULL, ses);
#endif
				goto x;
			case ACT_ADD_BOOKMARK_LINK:
#ifdef BOOKMARKS
				if (!get_opt_int_tree(cmdline_options, "anonymous"))
					launch_bm_add_link_dialog(ses->tab->term, NULL, ses);
#endif
				goto x;
			case ACT_BOOKMARK_MANAGER:
#ifdef BOOKMARKS
				if (!get_opt_int_tree(cmdline_options, "anonymous"))
					menu_bookmark_manager(ses->tab->term, NULL, ses);
#endif
				goto x;
			case ACT_HISTORY_MANAGER:
#ifdef GLOBHIST
				if (!get_opt_int_tree(cmdline_options, "anonymous"))
					menu_history_manager(ses->tab->term, NULL, ses);
#endif
				goto x;
			case ACT_OPTIONS_MANAGER:
				if (!get_opt_int_tree(cmdline_options, "anonymous"))
					menu_options_manager(ses->tab->term, NULL, ses);
				goto x;
			case ACT_KEYBINDING_MANAGER:
				if (!get_opt_int_tree(cmdline_options, "anonymous"))
					menu_keybinding_manager(ses->tab->term, NULL, ses);
				goto x;
			case ACT_COOKIES_LOAD:
#ifdef COOKIES
				if (!get_opt_int_tree(cmdline_options, "anonymous")
				    && get_opt_int("cookies.save"))
					load_cookies();
#endif
				goto x;
			case ACT_REALLY_QUIT:
				exit_prog(ses->tab->term, (void *)1, ses);
				goto x;
			case ACT_LUA_CONSOLE:
#ifdef HAVE_LUA
				trigger_event_name("dialog-lua-console", ses);
#endif
				goto x;
			case ACT_SCRIPTING_FUNCTION:
#ifdef HAVE_SCRIPTING
				trigger_event(func_ref, ses);
#endif
				break;
			case ACT_QUIT:

quit:
				exit_prog(ses->tab->term, (void *)(ev->x == KBD_CTRL_C), ses);
				goto x;
			case ACT_DOCUMENT_INFO:
				state_msg(ses);
				goto x;
			case ACT_HEADER_INFO:
				head_msg(ses);
				goto x;
			case ACT_TOGGLE_DISPLAY_IMAGES:
				toggle_images(ses, ses->doc_view, 0);
				goto x;
			case ACT_TOGGLE_DISPLAY_TABLES:
				get_opt_int("document.html.display_tables") =
					!get_opt_int("document.html.display_tables");
				html_interpret(ses);
				draw_formatted(ses);
				goto x;
			case ACT_TOGGLE_HTML_PLAIN:
				toggle_plain_html(ses, ses->doc_view, 0);
				goto x;
			case ACT_TOGGLE_NUMBERED_LINKS:
				toggle_link_numbering(ses, ses->doc_view, 0);
				goto x;
			case ACT_TOGGLE_DOCUMENT_COLORS:
				toggle_document_colors(ses, ses->doc_view, 0);
				goto x;
			case ACT_OPEN_NEW_WINDOW:
				open_in_new_window(ses->tab->term, send_open_new_window, ses);
				goto x;
			case ACT_OPEN_LINK_IN_NEW_WINDOW:
				if (!doc_view || doc_view->vs->current_link == -1) goto x;
				open_in_new_window(ses->tab->term, send_open_in_new_window, ses);
				goto x;
			case ACT_OPEN_NEW_TAB:
				open_in_new_tab(ses->tab->term, 0, ses);
				goto x;
			case ACT_OPEN_NEW_TAB_IN_BACKGROUND:
				open_in_new_tab_in_background(ses->tab->term, 0, ses);
				goto x;
			case ACT_OPEN_LINK_IN_NEW_TAB:
				open_in_new_tab(ses->tab->term, 1, ses);
				goto x;
			case ACT_OPEN_LINK_IN_NEW_TAB_IN_BACKGROUND:
				open_in_new_tab_in_background(ses->tab->term, 1, ses);
				goto x;
			case ACT_TAB_CLOSE:
				close_tab(ses->tab->term);
				ses = NULL; /* Disappeared in EV_ABORT handler. */
				goto x;
			case ACT_TAB_NEXT:
				switch_to_next_tab(ses->tab->term);
				goto x;
			case ACT_TAB_PREV:
				switch_to_prev_tab(ses->tab->term);
				goto x;

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
#ifdef USE_MOUSE
	if (ev->ev == EV_MOUSE) {
		int bars;
		int nb_tabs;

		if (ev->y == 0 && (ev->b & BM_ACT) == B_DOWN
		    && (ev->b & BM_BUTT) < B_WHEEL_UP) {
			struct window *m;

			activate_bfu_technology(ses, -1);
			m = ses->tab->term->windows.next;
			m->handler(m, ev, 0);
			goto x;
		}

		init_bars_status(ses, &nb_tabs, NULL);
		bars = 0;
		if (ses->visible_tabs_bar) bars++;
		if (ses->visible_status_bar) bars++;

		if (ev->y == ses->tab->term->y - bars && (ev->b & BM_ACT) == B_DOWN
		    && (ev->b & BM_BUTT) < B_WHEEL_UP) {
			int tab = get_tab_number_by_xpos(ses->tab->term, ev->x);

			if (tab != -1) switch_to_tab(ses->tab->term, tab, nb_tabs);
			goto x;
		}
		if (doc_view) do_mouse_event(ses, ev, doc_view);
	}
#endif /* USE_MOUSE */
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
send_enter_reload(struct terminal *term, void *xxx, struct session *ses)
{
	struct term_event ev = INIT_TERM_EVENT(EV_KBD, KBD_ENTER, KBD_CTRL, 0);

	assert(ses);
	if_assert_failed return;
	send_event(ses, &ev);
}

enum dl_type {
	URL,
	IMAGE,
};

static void
send_download_do(struct session *ses, enum dl_type dlt)
{
	struct document_view *doc_view;

	assert(ses);
	if_assert_failed return;
	doc_view = current_frame(ses);
	assert(doc_view && doc_view->vs && doc_view->document);
	if_assert_failed return;

	if (doc_view->vs->current_link == -1) return;
	if (ses->dn_url) {
		mem_free(ses->dn_url);
		ses->dn_url = NULL;
	}

	if (dlt == URL) {
		ses->dn_url = get_link_url(ses, doc_view, &doc_view->document->links[doc_view->vs->current_link]);
	} else if (dlt == IMAGE) {
		unsigned char *wi = doc_view->document->links[doc_view->vs->current_link].where_img;

		if (wi) ses->dn_url = stracpy(wi);
	} else {
		internal("Unknown dl_type");
		ses->dn_url = NULL;
		return;
	}

	if (ses->dn_url) {
		if (ses->ref_url) mem_free(ses->ref_url);
		ses->ref_url = stracpy(doc_view->document->url);
		query_file(ses, ses->dn_url, start_download, NULL, 1);
	}
}


void
send_download_image(struct terminal *term, void *xxx, struct session *ses)
{
	assert(term && ses);
	if_assert_failed return;
	send_download_do(ses, IMAGE);
}

void
send_download(struct terminal *term, void *xxx, struct session *ses)
{
	assert(term && ses);
	if_assert_failed return;
	send_download_do(ses, URL);
}

static struct string *
add_session_ring_to_string(struct string *str)
{
	int ring;

	assert(str);
	if_assert_failed return NULL;

	ring = get_opt_int_tree(cmdline_options, "session-ring");
	if (ring) {
		add_to_string(str, " -session-ring ");
		add_long_to_string(str, ring);
	}

	return str;
}

/* open a link in a new xterm */
void
send_open_in_new_window(struct terminal *term,
		       void (*open_window)(struct terminal *term, unsigned char *, unsigned char *),
		       struct session *ses)
{
	struct document_view *doc_view;

	assert(term && open_window && ses);
	if_assert_failed return;
	doc_view = current_frame(ses);
	assert(doc_view && doc_view->vs && doc_view->document);
	if_assert_failed return;

	if (doc_view->vs->current_link == -1) return;
	if (ses->dn_url) mem_free(ses->dn_url);
	ses->dn_url = get_link_url(ses, doc_view, &doc_view->document->links[doc_view->vs->current_link]);
	/* FIXME: We can't do this because ses->dn_url isn't alloc'd by init_string(). --pasky */
	/* if (ses->dn_url) add_session_ring_to_str(&ses->dn_url, &l); */
	if (ses->dn_url) {
		unsigned char *enc_url = encode_shell_safe_url(ses->dn_url);

		open_window(term, path_to_exe, enc_url);
		mem_free(enc_url);
	}
}

void
send_open_new_window(struct terminal *term,
		    void (*open_window)(struct terminal *, unsigned char *, unsigned char *),
		    struct session *ses)
{
	struct string dn_url;

	assert(term && open_window && ses);
	if_assert_failed return;

	if (ses->dn_url) mem_free(ses->dn_url);

	if (!init_string(&dn_url)) return;

	add_to_string(&dn_url, "-base-session ");
	add_long_to_string(&dn_url, ses->id);
	add_session_ring_to_string(&dn_url);

	ses->dn_url = dn_url.source;
	open_window(term, path_to_exe, ses->dn_url);
}

void
open_in_new_window(struct terminal *term,
		   void (*xxx)(struct terminal *,
			       void (*)(struct terminal *, unsigned char *, unsigned char *),
			       struct session *ses),
		   struct session *ses)
{
	struct menu_item *mi;
	struct open_in_new *oi, *oin;

	assert(term && ses && xxx);
	if_assert_failed return;

	oin = get_open_in_new(term->environment);
	if (!oin) return;
	if (!oin[1].text) {
		xxx(term, oin[0].fn, ses);
		mem_free(oin);
		return;
	}

	mi = new_menu(FREE_LIST);
	if (!mi) {
		mem_free(oin);
		return;
	}
	for (oi = oin; oi->text; oi++)
		add_to_menu(&mi, oi->text, "", (menu_func) xxx, oi->fn, 0, 0);
	mem_free(oin);
	do_menu(term, mi, ses, 1);
}

void
save_url(struct session *ses, unsigned char *url)
{
	struct document_view *doc_view;
	unsigned char *u;

	assert(ses && ses->tab && ses->tab->term && url);
	if_assert_failed return;
	if (!*url) return;
	u = translate_url(url, ses->tab->term->cwd);

	if (!u) {
		struct download stat = {
			NULL_LIST_HEAD, NULL, NULL, NULL, NULL, NULL,
			S_BAD_URL, PRI_CANCEL, 0
		};

		print_error_dialog(ses, &stat);
		return;
	}

	if (ses->dn_url) mem_free(ses->dn_url);
	ses->dn_url = u;

	if (ses->ref_url) mem_free(ses->ref_url);

	doc_view = current_frame(ses);
	assert(doc_view && doc_view->document && doc_view->document->url);
	if_assert_failed return;

	ses->ref_url = stracpy(doc_view->document->url);
	query_file(ses, ses->dn_url, start_download, NULL, 1);
}

void
send_image(struct terminal *term, void *xxx, struct session *ses)
{
	struct document_view *doc_view;
	unsigned char *u;

	assert(term && ses);
	if_assert_failed return;
	doc_view = current_frame(ses);
	assert(doc_view && doc_view->document && doc_view->vs);
	if_assert_failed return;

	if (doc_view->vs->current_link == -1) return;
	u = doc_view->document->links[doc_view->vs->current_link].where_img;
	if (!u) return;
	goto_url(ses, u);
}

void
save_as(struct terminal *term, void *xxx, struct session *ses)
{
	struct location *loc;

	assert(term && ses);
	if_assert_failed return;

	if (!have_location(ses)) return;
	loc = cur_loc(ses);
	if (ses->dn_url) mem_free(ses->dn_url);
	ses->dn_url = memacpy(loc->vs.url, loc->vs.url_len);
	if (ses->dn_url) {
		struct document_view *doc_view = current_frame(ses);

		assert(doc_view && doc_view->document && doc_view->document->url);
		if_assert_failed return;

		if (ses->ref_url) mem_free(ses->ref_url);
		ses->ref_url = stracpy(doc_view->document->url);
		query_file(ses, ses->dn_url, start_download, NULL, 1);
	}
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
save_formatted(struct session *ses, unsigned char *file)
{
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
menu_save_formatted(struct terminal *term, void *xxx, struct session *ses)
{
	struct document_view *doc_view;

	assert(term && ses);
	if_assert_failed return;
	if (!have_location(ses)) return;
	doc_view = current_frame(ses);
	assert(doc_view && doc_view->vs);
	if_assert_failed return;

	query_file(ses, doc_view->vs->url, save_formatted, NULL, !((int) xxx));
}


/* Print page's title and numbering at window top. */
unsigned char *
print_current_title(struct session *ses)
{
	struct document_view *doc_view;
	struct document *document;
	struct string title;
	unsigned char buf[80];
	int buflen = 0;
	int width;

	assert(ses && ses->tab && ses->tab->term);
	if_assert_failed return NULL;

	doc_view = current_frame(ses);

	assert(doc_view && doc_view->document);
	if_assert_failed return NULL;

	if (!init_string(&title)) return NULL;

	document = doc_view->document;
	width = ses->tab->term->x;

	/* Set up the document page info string: '(' %page '/' %pages ')' */
	if (doc_view->height < document->height) {
		int pos = doc_view->vs->y + doc_view->height;
		int page = 1;
		int pages = doc_view->height
			    ? (document->height + doc_view->height - 1) / doc_view->height
			    : 1;

		/* Check if at the end else calculate the page. */
		if (pos >= document->height) {
			page = pages;
		} else if (doc_view->height) {
			page = int_min((pos - doc_view->height / 2) / doc_view->height + 1,
				       pages);
		}

		buflen = snprintf(buf, sizeof(buf), " (%d/%d)", page, pages);
		if (buflen < 0) buflen = 0;
	}

	if (doc_view->document->title) {
		add_to_string(&title, doc_view->document->title);

		if (title.length + buflen > width - 4) {
			title.length = int_max(width - 4 - buflen, 0);
			add_to_string(&title, "...");
		}
	}

	if (buflen > 0)
		add_bytes_to_string(&title, buf, buflen);

	return title.source;
}
