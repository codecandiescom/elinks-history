/* HTML viewer (and much more) */
/* $Id: view.c,v 1.160 2003/07/17 08:56:33 zas Exp $ */

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
#include "document/options.h"
#include "document/html/parser.h"
#include "document/html/renderer.h"
#include "globhist/dialogs.h"
#include "intl/charsets.h"
#include "intl/gettext/libintl.h"
#include "protocol/http/auth.h"
#include "protocol/uri.h"
#include "sched/download.h"
#include "sched/history.h"
#include "sched/location.h"
#include "sched/session.h"
#include "scripting/lua/core.h"
#include "terminal/draw.h"
#include "terminal/kbd.h"
#include "terminal/tab.h"
#include "terminal/terminal.h"
#include "terminal/window.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memory.h"
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
init_formatted(struct document *scr)
{
	struct list_head tmp;

	memcpy(&tmp, (struct document **)scr, sizeof(struct list_head));
	memset(((struct document **)scr), 0, sizeof(struct document));
	memcpy((struct document **)scr, &tmp, sizeof(struct list_head));

	init_list(scr->forms);
	init_list(scr->tags);
	init_list(scr->nodes);
}

static void
free_frameset_desc(struct frameset_desc *fd)
{
	int i;

	for (i = 0; i < fd->n; i++) {
		if (fd->f[i].subframe) free_frameset_desc(fd->f[i].subframe);
		if (fd->f[i].name) mem_free(fd->f[i].name);
		if (fd->f[i].url) mem_free(fd->f[i].url);
	}
	mem_free(fd);
}

static void
clear_formatted(struct document *scr)
{
	int n;
	int y;
	struct cache_entry *ce;
	struct form_control *fc;

	assert(scr);
	if_assert_failed return;

	if (!find_in_cache(scr->url, &ce) || !ce)
		internal("no cache entry for document");
	else
		ce->refcount--;

	if (scr->url) mem_free(scr->url);
	if (scr->title) mem_free(scr->title);
	if (scr->frame_desc) {
		free_frameset_desc(scr->frame_desc);
	}
	for (n = 0; n < scr->nlinks; n++) {
		struct link *l = &scr->links[n];

		if (l->where) mem_free(l->where);
		if (l->target) mem_free(l->target);
		if (l->title) mem_free(l->title);
		if (l->where_img) mem_free(l->where_img);
		if (l->pos) mem_free(l->pos);
		if (l->name) mem_free(l->name);
	}
	if (scr->links) mem_free(scr->links);
	for (y = 0; y < scr->y; y++) if (scr->data[y].d) mem_free(scr->data[y].d);
	if (scr->data) mem_free(scr->data);
	if (scr->lines1) mem_free(scr->lines1);
	if (scr->lines2) mem_free(scr->lines2);
	if (scr->opt.framename) mem_free(scr->opt.framename);
	foreach (fc, scr->forms) {
		destroy_fc(fc);
	}
	free_list(scr->forms);
	free_list(scr->tags);
	free_list(scr->nodes);
	if (scr->search) mem_free(scr->search);
	if (scr->slines1) mem_free(scr->slines1);
	if (scr->slines2) mem_free(scr->slines2);
	init_formatted(scr);
}

void
destroy_formatted(struct document *scr)
{
	assert(scr);
	if_assert_failed return;
	assertm(!scr->refcount, "Attempt to free locked formatted data.");
	if_assert_failed return;

	clear_formatted(scr);
	del_from_list(scr);
	mem_free(scr);
}

void
detach_formatted(struct document_view *scr)
{
	assert(scr);
	if_assert_failed return;

	if (scr->document) {
		format_cache_reactivate(scr->document);
		if (!--scr->document->refcount) {
			format_cache_entries++;
			/*shrink_format_cache();*/
		}
		assertm(scr->document->refcount >= 0,
			"format_cache refcount underflow");
		if_assert_failed scr->document->refcount = 0;
		scr->document = NULL;
	}
	scr->vs = NULL;
	if (scr->link_bg) {
		mem_free(scr->link_bg), scr->link_bg = NULL;
		scr->link_bg_n = 0;
	}
	if (scr->name) {
		mem_free(scr->name), scr->name = NULL;
	}
}

static inline int
find_tag(struct document *f, unsigned char *name)
{
	struct tag *tag;

	foreach (tag, f->tags)
		if (!strcasecmp(tag->name, name))
			return tag->y;

	return -1;
}


unsigned char fr_trans[2][4] = {{0xb3, 0xc3, 0xb4, 0xc5}, {0xc4, 0xc2, 0xc1, 0xc5}};

/* 0 -> 1 <- 2 v 3 ^ */
enum xchar_dir {
	XD_RIGHT = 0,
	XD_LEFT,
	XD_DOWN,
	XD_UP
};

/* TODO: Whitespaces cleanup target. --pasky */
static void
set_xchar(struct terminal *t, int x, int y, enum xchar_dir dir)
{
       unsigned int c, d;

       assert(t);
	if_assert_failed return;

       if (x < 0 || x >= t->x || y < 0 || y >= t->y) return;

       c = get_char(t, x, y);
       if (!(c & ATTR_FRAME)) return;

       c &= 0xff;
       d = dir>>1;
       if (c == fr_trans[d][0])
	       set_only_char(t, x, y, fr_trans[d][1 + (dir & 1)] | ATTR_FRAME);
       else
	       if (c == fr_trans[d][2 - (dir & 1)])
		       set_only_char(t, x, y, fr_trans[d][3] | ATTR_FRAME);
}

static void
draw_frame_lines(struct terminal *t, struct frameset_desc *fsd, int xp, int yp)
{
	register int y, j;

	assert(t && fsd && fsd->f);
	if_assert_failed return;

	y = yp - 1;
	for (j = 0; j < fsd->y; j++) {
		register int x, i;
		int wwy = fsd->f[j * fsd->x].yw;

		x = xp - 1;
		for (i = 0; i < fsd->x; i++) {
			int wwx = fsd->f[i].xw;

			if (i) {
				fill_area(t, x, y + 1, 1, wwy, FRAMES_VLINE);
				if (j == fsd->y - 1)
					set_xchar(t, x, y + wwy + 1, XD_UP);
			} else if (j) {
				set_xchar(t, x, y, XD_RIGHT);
			}

			if (j) {
				fill_area(t, x + 1, y, wwx, 1, FRAMES_HLINE);
				if (i == fsd->x - 1)
					set_xchar(t, x + wwx + 1, y, XD_LEFT);
			} else if (i) {
				set_xchar(t, x, y, XD_DOWN);
			}

			if (i && j) set_char(t, x, y, FRAMES_CROSS);

			x += wwx + 1;
		}
		y += wwy + 1;
	}

	y = yp - 1;
	for (j = 0; j < fsd->y; j++) {
		register int x, i;
		int pj = j * fsd->x;
		int wwy = fsd->f[pj].yw;

		x = xp - 1;
		for (i = 0; i < fsd->x; i++) {
			int wwx = fsd->f[i].xw;
			int p = pj + i;

			if (fsd->f[p].subframe) {
				draw_frame_lines(t, fsd->f[p].subframe,
						 x + 1, y + 1);
			}
			x += wwx + 1;
		}
		y += wwy + 1;
	}
}

void
draw_doc(struct terminal *t, struct document_view *scr, int active)
{
	struct view_state *vs;
	int xp, yp;
	int xw, yw;
	int vx, vy;
	int y;

	assert(t && scr);
	if_assert_failed return;

	xp = scr->xp;
	yp = scr->yp;
	xw = scr->xw;
	yw = scr->yw;

	if (active) {
		set_cursor(t, xp + xw - 1, yp + yw - 1, xp + xw - 1, yp + yw - 1);
		set_window_ptr(get_current_tab(t), xp, yp);
	}
	if (!scr->vs) {
		fill_area(t, xp, yp, xw, yw, scr->document->y ? scr->document->bg : ' ');
		return;
	}
	if (scr->document->frame) {
	 	fill_area(t, xp, yp, xw, yw, scr->document->y ? scr->document->bg : ' ');
		draw_frame_lines(t, scr->document->frame_desc, xp, yp);
		if (scr->vs && scr->vs->current_link == -1) scr->vs->current_link = 0;
		return;
	}
	check_vs(scr);
	vs = scr->vs;
	if (vs->goto_position) {
		vy = find_tag(scr->document, vs->goto_position);
	       	if (vy != -1) {
			if (vy > scr->document->y) vy = scr->document->y - 1;
			if (vy < 0) vy = 0;
			vs->view_pos = vy;
			set_link(scr);
			mem_free(vs->goto_position);
			vs->goto_position = NULL;
		}
	}
	vx = vs->view_posx;
	vy = vs->view_pos;
	if (scr->xl == vx
	    && scr->yl == vy
	    && scr->xl != -1
	    && (!scr->search_word || !*scr->search_word || !(*scr->search_word)[0])) {
		clear_link(t, scr);
		draw_forms(t, scr);
		if (active) draw_current_link(t, scr);
		return;
	}
	free_link(scr);
	scr->xl = vx;
	scr->yl = vy;
	fill_area(t, xp, yp, xw, yw, scr->document->y ? scr->document->bg : ' ');
	if (!scr->document->y) return;

	while (vs->view_pos >= scr->document->y) vs->view_pos -= yw;
	if (vs->view_pos < 0) vs->view_pos = 0;
	if (vy != vs->view_pos) vy = vs->view_pos, check_vs(scr);
	for (y = vy <= 0 ? 0 : vy; y < (-vy + scr->document->y <= yw ? scr->document->y : yw + vy); y++) {
		int st = vx <= 0 ? 0 : vx;
		int en = -vx + scr->document->data[y].l <= xw ? scr->document->data[y].l : xw + vx;

		set_line(t, xp + st - vx, yp + y - vy, en - st, &scr->document->data[y].d[st]);
	}
	draw_forms(t, scr);
	if (active) draw_current_link(t, scr);
	if (scr->search_word && *scr->search_word && (*scr->search_word)[0]) scr->xl = scr->yl = -1;
}

static void
draw_frames(struct session *ses)
{
	struct document_view *f, *cf;
	int *l;
	int n, i, d, more;

	assert(ses && ses->screen && ses->screen->document);
	if_assert_failed return;

	if (!ses->screen->document->frame) return;
	n = 0;
	foreach (f, ses->scrn_frames) f->xl = f->yl = -1, n++;
	l = &cur_loc(ses)->vs.current_link;
	if (*l < 0) *l = 0;
	if (!n) n = 1;
	*l %= n;
	i = *l;
	cf = current_frame(ses);
	d = 0;
	do {
		more = 0;
		foreach (f, ses->scrn_frames) {
			if (f->depth == d)
				draw_doc(ses->tab->term, f, f == cf);
			else if (f->depth > d)
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

	if (!ses->screen || !ses->screen->document) {
		/*internal("document not formatted");*/
		fill_area(ses->tab->term, 0, 1, ses->tab->term->x, ses->tab->term->y - 2, ' ');
		return;
	}

	if (!ses->screen->vs && have_location(ses))
		ses->screen->vs = &cur_loc(ses)->vs;
	ses->screen->xl = ses->screen->yl = -1;
	draw_doc(ses->tab->term, ses->screen, 1);
	draw_frames(ses);
	print_screen_status(ses);
	redraw_from_window(ses->tab);
}

static void
page_down(struct session *ses, struct document_view *f, int a)
{
	int newpos;

	assert(ses && f && f->vs);
	if_assert_failed return;

	newpos = f->vs->view_pos + f->document->opt.yw;
	if (newpos < f->document->y) {
		f->vs->view_pos = newpos;
		find_link(f, 1, a);
	} else {
		find_link(f, -1, a);
	}
}

static void
page_up(struct session *ses, struct document_view *f, int a)
{
	assert(ses && f && f->vs);
	if_assert_failed return;

	f->vs->view_pos -= f->yw;
	find_link(f, -1, a);
	if (f->vs->view_pos < 0) f->vs->view_pos = 0/*, find_link(f, 1, a)*/;
}


void
down(struct session *ses, struct document_view *fd, int a)
{
	int current_link;

	assert(ses && fd && fd->vs && fd->document);
	if_assert_failed return;

	current_link = fd->vs->current_link;

	if (get_opt_int("document.browse.links.wraparound")
	    && current_link >= fd->document->nlinks - 1) {
		jump_to_link_number(ses, fd, 0);
		/* FIXME: This needs further work, we should call page_down()
		 * and set_textarea() under some conditions as well. --pasky */
		return;
	}

	if (current_link == -1
	    || !next_in_view(fd, current_link + 1, 1, in_viewy,
		    	     set_pos_x)) {
		page_down(ses, fd, 1);
	}

	if (current_link != fd->vs->current_link) {
		set_textarea(ses, fd, KBD_UP);
	}
}

static void
up(struct session *ses, struct document_view *fd, int a)
{
	int current_link;

	assert(ses && fd && fd->vs && fd->document);
	if_assert_failed return;

	current_link = fd->vs->current_link;

	if (get_opt_int("document.browse.links.wraparound")
	    && current_link == 0) {
		jump_to_link_number(ses, fd, fd->document->nlinks - 1);
		/* FIXME: This needs further work, we should call page_down()
		 * and set_textarea() under some conditions as well. --pasky */
		return;
	}

	if (current_link == -1
	    || !next_in_view(fd, current_link - 1, -1, in_viewy,
		    	     set_pos_x)) {
		page_up(ses, fd, 1);
	}

	if (current_link != fd->vs->current_link) {
		set_textarea(ses, fd, KBD_DOWN);
	}
}


#define scroll scroll_dirty_workaround_for_name_clash_with_libraries_on_macos

static void
scroll(struct session *ses, struct document_view *f, int a)
{
	assert(ses && f && f->vs && f->document);
	if_assert_failed return;

	if (f->vs->view_pos + f->document->opt.yw >= f->document->y && a > 0)
		return;
	f->vs->view_pos += a;
	if (f->vs->view_pos > f->document->y - f->document->opt.yw && a > 0)
		f->vs->view_pos = f->document->y - f->document->opt.yw;
	if (f->vs->view_pos < 0) f->vs->view_pos = 0;
	if (c_in_view(f)) return;
	find_link(f, a < 0 ? -1 : 1, 0);
}

static void
hscroll(struct session *ses, struct document_view *f, int a)
{
	assert(ses && f && f->vs && f->document);
	if_assert_failed return;

	f->vs->view_posx += a;
	if (f->vs->view_posx >= f->document->x)
		f->vs->view_posx = f->document->x - 1;
	if (f->vs->view_posx < 0) f->vs->view_posx = 0;
	if (c_in_view(f)) return;
	find_link(f, 1, 0);
	/* !!! FIXME: check right margin */
}

static void
home(struct session *ses, struct document_view *f, int a)
{
	assert(ses && f && f->vs);
	if_assert_failed return;

	f->vs->view_pos = f->vs->view_posx = 0;
	find_link(f, 1, 0);
}

static void
x_end(struct session *ses, struct document_view *f, int a)
{
	assert(ses && f && f->vs && f->document);
	if_assert_failed return;

	f->vs->view_posx = 0;
	if (f->vs->view_pos < f->document->y - f->document->opt.yw)
		f->vs->view_pos = f->document->y - f->document->opt.yw;
	if (f->vs->view_pos < 0) f->vs->view_pos = 0;
	find_link(f, -1, 0);
}

inline void
decrement_fc_refcount(struct document *f)
{
	assert(f);
	if_assert_failed return;

	if (!--f->refcount) format_cache_entries++;
	assertm(f->refcount >= 0, "reference count underflow");
	if_assert_failed f->refcount = 0;
}


void
set_frame(struct session *ses, struct document_view *f, int a)
{
	assert(ses && ses->screen && f && f->vs);
	if_assert_failed return;

	if (f == ses->screen) return;
	goto_url(ses, f->vs->url);
}


void
toggle(struct session *ses, struct document_view *f, int a)
{
	assert(ses && f && ses->tab && ses->tab->term);
	if_assert_failed return;

	if (!f->vs) {
		nowhere_box(ses->tab->term, NULL);
		return;
	}

	f->vs->plain = !f->vs->plain;
	html_interpret(ses);
	draw_formatted(ses);
}


static inline void
rep_ev(struct session *ses, struct document_view *fd,
       void (*f)(struct session *, struct document_view *, int),
       int a)
{
	register int i;

	assert(ses && fd && f);
	if_assert_failed return;

	i = ses->kbdprefix.rep ? ses->kbdprefix.rep_num : 1;
	while (i--) f(ses, fd, a);
}


void frm_download(struct session *, struct document_view *, int resume);

static int
frame_ev(struct session *ses, struct document_view *fd, struct event *ev)
{
	int x = 1;

	assert(ses && fd && fd->document && fd->vs && ev);
	if_assert_failed return 1;

	if (fd->vs->current_link >= 0
	    && (fd->document->links[fd->vs->current_link].type == L_FIELD ||
		fd->document->links[fd->vs->current_link].type == L_AREA)
	    && field_op(ses, fd, &fd->document->links[fd->vs->current_link], ev, 0))
		return 1;

	if (ev->ev == EV_KBD) {
		if (ev->x >= '0' + !ses->kbdprefix.rep && ev->x <= '9'
		    && (ev->y
			|| !fd->document->opt.num_links_key
			|| (fd->document->opt.num_links_key == 1
			    && !fd->document->opt.num_links_display))) {
			/* Repeat count */

			if (!ses->kbdprefix.rep)
				ses->kbdprefix.rep_num = 0;

			ses->kbdprefix.rep_num = ses->kbdprefix.rep_num * 10
						 + ev->x - '0';
			if (ses->kbdprefix.rep_num > 65536)
				ses->kbdprefix.rep_num = 65536;

			ses->kbdprefix.rep = 1;
			return 2;
		}

		if (get_opt_int("document.browse.accesskey.priority") >= 2
		    && try_document_key(ses, fd, ev)) {
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
				if (!ses->kbdprefix.rep) break;

				if (ses->kbdprefix.rep_num
				    > fd->document->nlinks) {
					ses->kbdprefix.rep = 0;
					return 2;
				}

				jump_to_link_number(ses,
						    current_frame(ses),
						    ses->kbdprefix.rep_num
							- 1);
		}

		switch (kbd_action(KM_MAIN, ev, NULL)) {
			case ACT_PAGE_DOWN: rep_ev(ses, fd, page_down, 0); break;
			case ACT_PAGE_UP: rep_ev(ses, fd, page_up, 0); break;
			case ACT_DOWN: rep_ev(ses, fd, down, 0); break;
			case ACT_UP: rep_ev(ses, fd, up, 0); break;
			case ACT_COPY_CLIPBOARD: {
				char *current_link = print_current_link(ses);

				if (current_link) {
					set_clipboard_text(current_link);
					mem_free(current_link);
				}
				break;
			}

	     		/* XXX: Code duplication of following for mouse */
			case ACT_SCROLL_UP: scroll(ses, fd, ses->kbdprefix.rep ? -ses->kbdprefix.rep_num : -get_opt_int("document.browse.scroll_step")); break;
			case ACT_SCROLL_DOWN: scroll(ses, fd, ses->kbdprefix.rep ? ses->kbdprefix.rep_num : get_opt_int("document.browse.scroll_step")); break;
			case ACT_SCROLL_LEFT: rep_ev(ses, fd, hscroll, -1 - 7 * !ses->kbdprefix.rep); break;
			case ACT_SCROLL_RIGHT: rep_ev(ses, fd, hscroll, 1 + 7 * !ses->kbdprefix.rep); break;

			case ACT_HOME: rep_ev(ses, fd, home, 0); break;
			case ACT_END:  rep_ev(ses, fd, x_end, 0); break;
			case ACT_ENTER: x = enter(ses, fd, 0); if (x == 2 && ses->kbdprefix.rep) x = 1; break;
			case ACT_ENTER_RELOAD: x = enter(ses, fd, 1); if (x == 2 && ses->kbdprefix.rep) x = 1; break;
			case ACT_DOWNLOAD: if (!get_opt_int_tree(cmdline_options, "anonymous")) frm_download(ses, fd, 0); break;
			case ACT_RESUME_DOWNLOAD: if (!get_opt_int_tree(cmdline_options, "anonymous")) frm_download(ses, fd, 1); break;
			case ACT_SEARCH: search_dlg(ses, fd, 0); break;
			case ACT_SEARCH_BACK: search_back_dlg(ses, fd, 0); break;
			case ACT_FIND_NEXT: find_next(ses, fd, 0); break;
			case ACT_FIND_NEXT_BACK: find_next_back(ses, fd, 0); break;
			case ACT_ZOOM_FRAME: set_frame(ses, fd, 0), x = 2; break;
			case ACT_VIEW_IMAGE: send_image(ses->tab->term, NULL, ses); break;
			case ACT_DOWNLOAD_IMAGE: send_download_image(ses->tab->term, NULL, ses); break;
			case ACT_LINK_MENU: link_menu(ses->tab->term, NULL, ses); break;
			case ACT_JUMP_TO_LINK: break;
			default:
				if (ev->x >= '1' && ev->x <= '9' && !ev->y) {
					/* FIXME: This probably doesn't work
					 * together with the keybinding...? */

					struct document *document = fd->document;
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
								int rx = x + node->x + fd->xp - fd->vs->view_posx;
								int ry = y + node->y + fd->yp - fd->vs->view_pos;
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
					 && try_document_key(ses, fd, ev)) {
					/* The document ate the key! */
					return 1;

				} else {
					x = 0;
				}
		}

	} else if (ev->ev == EV_MOUSE) {
		struct link *link = choose_mouse_link(fd, ev);

		if ((ev->b & BM_BUTT) >= B_WHEEL_UP) {
			if ((ev->b & BM_ACT) != B_DOWN) {
				/* We handle only B_DOWN case... */
			} else if ((ev->b & BM_BUTT) == B_WHEEL_UP) {
				rep_ev(ses, fd, scroll, -2);
			} else if ((ev->b & BM_BUTT) == B_WHEEL_DOWN) {
				rep_ev(ses, fd, scroll, 2);
			}

		} else if (link) {
			x = 1;
			fd->vs->current_link = link - fd->document->links;

			if ((link->type == L_LINK || link->type == L_BUTTON ||
			     link->type == L_CHECKBOX || link->type == L_SELECT)
			    && (ev->b & BM_ACT) == B_UP) {

				draw_doc(ses->tab->term, fd, 1);
				print_screen_status(ses);
				redraw_from_window(ses->tab);

				if ((ev->b & BM_BUTT) < B_MIDDLE)
					x = enter(ses, fd, 0);
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
				rep_ev(ses, fd, scroll, -2);
			}
			if (ev->y >= fd->yw - scrollmargin) {
				rep_ev(ses, fd, scroll, 2);
			}

			if (ev->x < scrollmargin * 2) {
				rep_ev(ses, fd, hscroll, -8);
			}
			if (ev->x >= fd->xw - scrollmargin * 2) {
				rep_ev(ses, fd, hscroll, 8);
			}
		}

	} else {
		x = 0;
	}

	ses->kbdprefix.rep = 0;
	return x;
}

struct document_view *
current_frame(struct session *ses)
{
	struct document_view *fd = NULL;
	int i;

	assert(ses);
	if_assert_failed return NULL;

	if (!have_location(ses)) return NULL;
	i = cur_loc(ses)->vs.current_link;
	foreach (fd, ses->scrn_frames) {
		if (fd->document && fd->document->frame) continue;
		if (!i--) return fd;
	}
	fd = cur_loc(ses)->vs.view;
	/* The fd test probably only hides bugs in history handling. --pasky */
	if (/*fd &&*/ fd->document && fd->document->frame) return NULL;
	return fd;
}

static int
send_to_frame(struct session *ses, struct event *ev)
{
	struct document_view *fd;
	int r;

	assert(ses && ses->tab && ses->tab->term && ev);
	if_assert_failed return 0;
	fd = current_frame(ses);
	assertm(fd, "document not formatted");
	if_assert_failed return 0;

	r = frame_ev(ses, fd, ev);
	if (r == 1) {
		draw_doc(ses->tab->term, fd, 1);
		print_screen_status(ses);
		redraw_from_window(ses->tab);
	}

	return r;
}

void
do_for_frame(struct session *ses,
	     void (*f)(struct session *, struct document_view *, int),
	     int a)
{
	struct document_view *fd;

	assert(ses && f);
	if_assert_failed return;
	fd = current_frame(ses);
	assertm(fd, "document not formatted");
	if_assert_failed return;

	f(ses, fd, a);
}

static void
do_mouse_event(struct session *ses, struct event *ev)
{
	struct event evv;
	struct document_view *fdd, *fd; /* !!! FIXME: frames */
	struct document_options *o;

	assert(ses && ev);
	if_assert_failed return;
	fd = current_frame(ses);
	assert(fd && fd->document);
	if_assert_failed return;

	o = &fd->document->opt;
	if (ev->x >= o->xp && ev->x < o->xp + o->xw &&
	    ev->y >= o->yp && ev->y < o->yp + o->yw) goto ok;

r:
	next_frame(ses, 1);
	fdd = current_frame(ses);
	assert(fdd && fdd->document);
	if_assert_failed return;
	o = &fdd->document->opt;
	if (ev->x >= o->xp && ev->x < o->xp + o->xw &&
	    ev->y >= o->yp && ev->y < o->yp + o->yw) {
		draw_formatted(ses);
		fd = fdd;
		goto ok;
	}
	if (fdd != fd) goto r;
	return;

ok:
	memcpy(&evv, ev, sizeof(struct event));
	evv.x -= fd->xp;
	evv.y -= fd->yp;
	send_to_frame(ses, &evv);
}

void send_open_in_new_xterm(struct terminal *, void (*)(struct terminal *, unsigned char *, unsigned char *), struct session *);

void
send_event(struct session *ses, struct event *ev)
{
	struct document_view *fd;

	assert(ses && ev);
	if_assert_failed return;
	fd = current_frame(ses);

	if (ev->ev == EV_KBD) {
		int func_ref;

		if (fd && send_to_frame(ses, ev)) return;

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

				if (!have_location(ses)) goto quak;
				s = stracpy(cur_loc(ses)->vs.url);
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
				dialog_lua_console(ses);
#endif
				goto x;
			case ACT_LUA_FUNCTION:
#ifdef HAVE_LUA
				run_lua_func(ses, func_ref);
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
				get_opt_int("document.browse.images.show_as_links") =
					!get_opt_int("document.browse.images.show_as_links");
				html_interpret(ses);
				draw_formatted(ses);
				goto x;
			case ACT_TOGGLE_DISPLAY_TABLES:
				get_opt_int("document.html.display_tables") =
					!get_opt_int("document.html.display_tables");
				html_interpret(ses);
				draw_formatted(ses);
				goto x;
			case ACT_TOGGLE_HTML_PLAIN:
				toggle(ses, ses->screen, 0);
				goto x;
			case ACT_TOGGLE_NUMBERED_LINKS:
				get_opt_int("document.browse.links.numbering") =
					!get_opt_int("document.browse.links.numbering");
				html_interpret(ses);
				draw_formatted(ses);
				goto x;
			case ACT_OPEN_NEW_WINDOW:
				open_in_new_window(ses->tab->term, send_open_new_xterm, ses);
				goto x;
			case ACT_OPEN_LINK_IN_NEW_WINDOW:
				if (!fd || fd->vs->current_link == -1) goto x;
				open_in_new_window(ses->tab->term, send_open_in_new_xterm, ses);
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

		if (fd
		    && get_opt_int("document.browse.accesskey.priority") <= 0
		    && try_document_key(ses, fd, ev)) {
			/* The document ate the key! */
			draw_doc(ses->tab->term, fd, 1);
			print_screen_status(ses);
			redraw_from_window(ses->tab);
			return;
		}
	}

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
			int tab_width = ses->tab->term->x / nb_tabs;
			int tab = ev->x / tab_width;

			if (tab < 0) tab = 0;
			if (tab >= nb_tabs)
				tab = nb_tabs - 1;

			switch_to_tab(ses->tab->term, tab, nb_tabs);
			goto x;
		}
		if (fd) do_mouse_event(ses, ev);
	}

	return;

x:
	/* ses may disappear ie. in close_tab() */
	if (ses) {
		ses->kbdprefix.rep = 0;
	}
}

void
send_enter(struct terminal *term, void *xxx, struct session *ses)
{
	struct event ev = { EV_KBD, KBD_ENTER, 0, 0 };

	assert(ses);
	if_assert_failed return;
	send_event(ses, &ev);
}

void
send_enter_reload(struct terminal *term, void *xxx, struct session *ses)
{
	struct event ev = { EV_KBD, KBD_ENTER, KBD_CTRL, 0 };

	assert(ses);
	if_assert_failed return;
	send_event(ses, &ev);
}

void
frm_download(struct session *ses, struct document_view *fd, int resume)
{
	struct link *link;

	assert(ses && fd && fd->vs && fd->document);
	if_assert_failed return;

	if (fd->vs->current_link == -1) return;
	if (ses->dn_url) {
		mem_free(ses->dn_url);
		ses->dn_url = NULL;
	}
	link = &fd->document->links[fd->vs->current_link];
	if (link->type != L_LINK && link->type != L_BUTTON) return;

	ses->dn_url = get_link_url(ses, fd, link);
	if (ses->dn_url) {
		if (!strncasecmp(ses->dn_url, "MAP@", 4)) {
			mem_free(ses->dn_url);
			ses->dn_url = NULL;
			return;
		}
		if (ses->ref_url) mem_free(ses->ref_url);
		ses->ref_url = stracpy(fd->document->url);
		query_file(ses, ses->dn_url, (resume ? resume_download : start_download), NULL, 1);
	}
}

enum dl_type {
	URL,
	IMAGE,
};

static void
send_download_do(struct terminal *term, void *xxx, struct session *ses,
		enum dl_type dlt)
{
	struct document_view *fd;

	assert(term && ses);
	if_assert_failed return;
	fd = current_frame(ses);
	assert(fd && fd->vs && fd->document);
	if_assert_failed return;

	if (fd->vs->current_link == -1) return;
	if (ses->dn_url) {
		mem_free(ses->dn_url);
		ses->dn_url = NULL;
	}

	if (dlt == URL) {
		ses->dn_url = get_link_url(ses, fd, &fd->document->links[fd->vs->current_link]);
	} else if (dlt == IMAGE) {
		unsigned char *wi = fd->document->links[fd->vs->current_link].where_img;

		if (wi) ses->dn_url = stracpy(wi);
	} else {
		internal("Unknown dl_type");
		ses->dn_url = NULL;
		return;
	}

	if (ses->dn_url) {
		if (ses->ref_url) mem_free(ses->ref_url);
		ses->ref_url = stracpy(fd->document->url);
		query_file(ses, ses->dn_url, start_download, NULL, 1);
	}
}


void
send_download_image(struct terminal *term, void *xxx, struct session *ses)
{
	assert(term && ses);
	if_assert_failed return;
	send_download_do(term, xxx, ses, IMAGE);
}

void
send_download(struct terminal *term, void *xxx, struct session *ses)
{
	assert(term && ses);
	if_assert_failed return;
	send_download_do(term, xxx, ses, URL);
}

static int
add_session_ring_to_str(unsigned char **str, int *len)
{
	int ring;

	assert(str && len);
	if_assert_failed return 0;

	ring = get_opt_int_tree(cmdline_options, "session-ring");
	if (ring) {
		add_to_str(str, len, " -session-ring ");
		add_num_to_str(str, len, ring);
	}

	return ring;
}

/* open a link in a new xterm */
void
send_open_in_new_xterm(struct terminal *term,
		       void (*open_window)(struct terminal *term, unsigned char *, unsigned char *),
		       struct session *ses)
{
	struct document_view *fd;

	assert(term && open_window && ses);
	if_assert_failed return;
	fd = current_frame(ses);
	assert(fd && fd->vs && fd->document);
	if_assert_failed return;

	if (fd->vs->current_link == -1) return;
	if (ses->dn_url) mem_free(ses->dn_url);
	ses->dn_url = get_link_url(ses, fd, &fd->document->links[fd->vs->current_link]);
	/* FIXME: We can't do this because ses->dn_url isn't alloc'd by init_str(). --pasky */
	/* if (ses->dn_url) add_session_ring_to_str(&ses->dn_url, &l); */
	if (ses->dn_url) {
		unsigned char *enc_url = encode_shell_safe_url(ses->dn_url);

		open_window(term, path_to_exe, enc_url);
		mem_free(enc_url);
	}
}

void
send_open_new_xterm(struct terminal *term,
		    void (*open_window)(struct terminal *, unsigned char *, unsigned char *),
		    struct session *ses)
{
	int l = 0;

	assert(term && open_window && ses);
	if_assert_failed return;

	if (ses->dn_url) mem_free(ses->dn_url);
	ses->dn_url = init_str();
	if (!ses->dn_url) return;
	add_to_str(&ses->dn_url, &l, "-base-session ");
	add_num_to_str(&ses->dn_url, &l, ses->id);
	add_session_ring_to_str(&ses->dn_url, &l);
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
		add_to_menu(&mi, oi->text, "", MENU_FUNC xxx, oi->fn, 0, 0);
	mem_free(oin);
	do_menu(term, mi, ses, 1);
}

void
save_url(struct session *ses, unsigned char *url)
{
	struct document_view *fd;
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

	fd = current_frame(ses);
	assert(fd && fd->document && fd->document->url);
	if_assert_failed return;

	ses->ref_url = stracpy(fd->document->url);
	query_file(ses, ses->dn_url, start_download, NULL, 1);
}

void
send_image(struct terminal *term, void *xxx, struct session *ses)
{
	struct document_view *fd;
	unsigned char *u;

	assert(term && ses);
	if_assert_failed return;
	fd = current_frame(ses);
	assert(fd && fd->document && fd->vs);
	if_assert_failed return;

	if (fd->vs->current_link == -1) return;
	u = fd->document->links[fd->vs->current_link].where_img;
	if (!u) return;
	goto_url(ses, u);
}

void
save_as(struct terminal *term, void *xxx, struct session *ses)
{
	struct location *l;

	assert(term && ses);
	if_assert_failed return;

	if (!have_location(ses)) return;
	l = cur_loc(ses);
	if (ses->dn_url) mem_free(ses->dn_url);
	ses->dn_url = stracpy(l->vs.url);
	if (ses->dn_url) {
		struct document_view *fd = current_frame(ses);

		assert(fd && fd->document && fd->document->url);
		if_assert_failed return;

		if (ses->ref_url) mem_free(ses->ref_url);
		ses->ref_url = stracpy(fd->document->url);
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
			N_("Cancel"), NULL, B_ENTER | B_ESC);
	}
	close(h);
}

static void
save_formatted(struct session *ses, unsigned char *file)
{
	struct document_view *fd;

	assert(ses && ses->tab && ses->tab->term && file);
	if_assert_failed return;
	fd = current_frame(ses);
	assert(fd && fd->document);
	if_assert_failed return;

	create_download_file(ses->tab->term, file, NULL, 0, 0,
			     save_formatted_finish, fd->document);
}

void
menu_save_formatted(struct terminal *term, void *xxx, struct session *ses)
{
	struct document_view *fd;

	assert(term && ses);
	if_assert_failed return;
	if (!have_location(ses)) return;
	fd = current_frame(ses);
	assert(fd && fd->vs);
	if_assert_failed return;

	query_file(ses, fd->vs->url, save_formatted, NULL, !((int) xxx));
}


/* Print page's title and numbering at window top. */
static unsigned char *
print_current_titlex(struct document_view *fd, int w)
{
	int ml = 0, pl = 0;
	unsigned char *m;
	unsigned char *p;

	assert(fd);
	if_assert_failed return NULL;

	p = init_str();
	if (!p) return NULL;

	if (fd->yw < fd->document->y) {
		int pp = 1;
		int pe = 1;

		if (fd->yw) {
			pp = (fd->vs->view_pos + fd->yw / 2) / fd->yw + 1;
			pe = (fd->document->y + fd->yw - 1) / fd->yw;
			if (pp > pe) pp = pe;
		}

		if (fd->vs->view_pos + fd->yw >= fd->document->y)
			pp = pe;
		if (fd->document->title)
			add_chr_to_str(&p, &pl, ' ');

		add_chr_to_str(&p, &pl, '(');
		add_num_to_str(&p, &pl, pp);
		add_chr_to_str(&p, &pl, '/');
		add_num_to_str(&p, &pl, pe);
		add_chr_to_str(&p, &pl, ')');
	}

	if (!fd->document->title) return p;

	m = init_str();
	if (!m) goto end;

	add_to_str(&m, &ml, fd->document->title);

	if (ml + pl > w - 4) {
		ml = w - 4 - pl;
		if (ml < 0) ml = 0;
		add_to_str(&m, &ml, "...");

	}

	add_to_str(&m, &ml, p);

end:
	mem_free(p);

	return m;
}

unsigned char *
print_current_title(struct session *ses)
{
	struct document_view *fd;

	assert(ses && ses->tab && ses->tab->term);
	if_assert_failed return NULL;
	fd = current_frame(ses);
	assert(fd);
	if_assert_failed return NULL;

	return print_current_titlex(fd, ses->tab->term->x);
}
