/* Links viewing/manipulation handling */
/* $Id: link.c,v 1.19 2003/07/27 20:20:13 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "bfu/menu.h"
#include "bookmarks/dialogs.h"
#include "document/html/parser.h"
#include "document/html/renderer.h"
#include "intl/gettext/libintl.h"
#include "protocol/uri.h"
#include "sched/session.h"
#include "terminal/draw.h"
#include "terminal/kbd.h"
#include "terminal/tab.h"
#include "terminal/terminal.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/form.h"
#include "viewer/text/link.h"
#include "viewer/text/search.h"
#include "viewer/text/textarea.h"
#include "viewer/text/view.h"
#include "viewer/text/vs.h"


/* Perhaps some of these would be more fun to have in viewere/common/, dunno.
 * --pasky */

/* FIXME: Add comments!! --Zas */


void
set_link(struct document_view *f)
{
	assert(f);
	if_assert_failed return;
	if (c_in_view(f)) return;
	find_link(f, 1, 0);
}


static int
comp_links(struct link *l1, struct link *l2)
{
	assert(l1 && l2);
	if_assert_failed return 0;
	return (l1->num - l2->num);
}

void
sort_links(struct document *f)
{
	int i;

	assert(f);
	if_assert_failed return;
	if (!f->nlinks) return;

	assert(f->links);
	if_assert_failed return;

	qsort(f->links, f->nlinks, sizeof(struct link),
	      (void *) comp_links);

	if (!f->y) return;

	f->lines1 = mem_calloc(f->y, sizeof(struct link *));
	if (!f->lines1) return;
	f->lines2 = mem_calloc(f->y, sizeof(struct link *));
	if (!f->lines2) {
		mem_free(f->lines1);
		return;
	}

	for (i = 0; i < f->nlinks; i++) {
		struct link *link = &f->links[i];
		register int p, q, j;

		if (!link->n) {
			if (link->where) mem_free(link->where);
			if (link->target) mem_free(link->target);
			if (link->title) mem_free(link->title);
			if (link->where_img) mem_free(link->where_img);
			if (link->pos) mem_free(link->pos);
			if (link->name) mem_free(link->name);
			memmove(link, link + 1,
				(f->nlinks - i - 1) * sizeof(struct link));
			f->nlinks--;
			i--;
			continue;
		}
		p = link->pos[0].y;
		q = link->pos[link->n - 1].y;
		if (p > q) j = p, p = q, q = j;
		for (j = p; j <= q; j++) {
			if (j >= f->y) {
				internal("link out of screen");
				continue;
			}
			f->lines2[j] = &f->links[i];
			if (!f->lines1[j]) f->lines1[j] = &f->links[i];
		}
	}
}


static void
draw_link(struct terminal *t, struct document_view *scr, int l)
{
	struct link *link;
	struct view_state *vs;
	int xp, yp;
	int xw, yw;
	int vx, vy;
	int f = 0;

	assert(t && scr && scr->vs);
	if_assert_failed return;
	assertm(!scr->link_bg, "link background not empty");
	if_assert_failed mem_free(scr->link_bg);

	if (l == -1) return;

	link = &scr->document->links[l];
	xp = scr->xp;
	yp = scr->yp;
	xw = scr->xw;
	yw = scr->yw;
	vs = scr->vs;
	vx = vs->view_posx;
	vy = vs->view_pos;

	switch (link->type) {
		int i;
		int q;

		case L_LINK:
		case L_CHECKBOX:
		case L_BUTTON:
		case L_SELECT:
		case L_FIELD:
		case L_AREA:
			q = 0;
			if (link->type == L_FIELD) {
				struct form_state *fs = find_form_state(scr, link->form);

				if (fs) q = fs->state - fs->vpos;
				/*else internal("link has no form control");*/
			} else if (link->type == L_AREA) {
				struct form_state *fs = find_form_state(scr, link->form);

				if (fs) q = area_cursor(link->form, fs);
				/*else internal("link has no form control");*/
			}

			scr->link_bg = mem_alloc(link->n * sizeof(struct link_bg));
			if (!scr->link_bg) return;
			scr->link_bg_n = link->n;

			for (i = 0; i < link->n; i++) {
				int x = link->pos[i].x + xp - vx;
				int y = link->pos[i].y + yp - vy;

				if (x >= xp && y >= yp && x < xp+xw && y < yp+yw) {
					unsigned co = get_char(t, x, y);

					if (scr->link_bg) {
						scr->link_bg[i].x = x;
						scr->link_bg[i].y = y;
						scr->link_bg[i].c = co;
					}
					if (!f
					    || (link->type == L_CHECKBOX && i == 1)
					    || (link->type == L_BUTTON && i == 2)
					    || ((link->type == L_FIELD ||
						 link->type == L_AREA) && i == q)) {
						int blockable = 0;

						if (link->type != L_FIELD && link->type != L_AREA) {
							if (((co >> 8) & 0x38) != (link->sel_color & 0x38)) {
								blockable = 1;
							}
						}

						set_cursor(t, x, y, blockable);
						set_window_ptr(get_current_tab(t), x, y);
						f = 1;
					}
					set_color(t, x, y, /*((link->sel_color << 3) | (co >> 11 & 7)) << 8*/ link->sel_color << 8);
				} else scr->link_bg[i].x = scr->link_bg[i].y = scr->link_bg[i].c = -1;
			}
			break;
		default:
			internal("bad link type");
	}
}


void
free_link(struct document_view *scr)
{
	assert(scr);
	if_assert_failed return;

	if (scr->link_bg) mem_free(scr->link_bg), scr->link_bg = NULL;
	scr->link_bg_n = 0;
}


void
clear_link(struct terminal *t, struct document_view *scr)
{
	assert(t && scr);
	if_assert_failed return;

	if (scr->link_bg) {
		struct link_bg *link_bg = scr->link_bg;
		int i;

		for (i = scr->link_bg_n - 1; i >= 0; i--) {
			struct link_bg *bgchar = &link_bg[i];

			if (bgchar->x != -1 && bgchar->y != -1)
				set_char(t, bgchar->x, bgchar->y, bgchar->c);
		}
		free_link(scr);
	}
}


void
draw_current_link(struct terminal *t, struct document_view *scr)
{
	assert(t && scr && scr->vs);
	if_assert_failed return;

	draw_link(t, scr, scr->vs->current_link);
	draw_searched(t, scr);
}


struct link *
get_first_link(struct document_view *f)
{
	struct link *l;
	register int i;

	assert(f && f->document);
	if_assert_failed return NULL;

	if (!f->document->lines1) return NULL;

	l = f->document->links + f->document->nlinks;

	for (i = f->vs->view_pos; i < f->vs->view_pos + f->yw; i++)
		if (i >= 0 && i < f->document->y && f->document->lines1[i]
		    && f->document->lines1[i] < l)
			l = f->document->lines1[i];

	if (l == f->document->links + f->document->nlinks) l = NULL;
	return l;
}

struct link *
get_last_link(struct document_view *f)
{
	struct link *l = NULL;
	register int i;

	assert(f && f->document);
	if_assert_failed return NULL;

	if (!f->document->lines2) return NULL;

	for (i = f->vs->view_pos; i < f->vs->view_pos + f->yw; i++)
		if (i >= 0 && i < f->document->y && f->document->lines2[i] > l)
			l = f->document->lines2[i];
	return l;
}


static int
in_viewx(struct document_view *f, struct link *l)
{
	register int i;

	assert(f && l);
	if_assert_failed return 0;

	for (i = 0; i < l->n; i++) {
		if (l->pos[i].x >= f->vs->view_posx
		    && l->pos[i].x < f->vs->view_posx + f->xw)
			return 1;
	}
	return 0;
}

int
in_viewy(struct document_view *f, struct link *l)
{
	register int i;

	assert(f && l);
	if_assert_failed return 0;

	for (i = 0; i < l->n; i++) {
		if (l->pos[i].y >= f->vs->view_pos
		    && l->pos[i].y < f->vs->view_pos + f->yw)
			return 1;
	}
	return 0;
}

int
in_view(struct document_view *f, struct link *l)
{
	assert(f && l);
	if_assert_failed return 0;
	return in_viewy(f, l) && in_viewx(f, l);
}

int
c_in_view(struct document_view *f)
{
	assert(f && f->vs);
	if_assert_failed return 0;
	return (f->vs->current_link != -1
		&& in_view(f, &f->document->links[f->vs->current_link]));
}

int
next_in_view(struct document_view *f, int p, int d,
	     int (*fn)(struct document_view *, struct link *),
	     void (*cntr)(struct document_view *, struct link *))
{
	int p1, p2 = 0;
	int y, yl;

	assert(f && f->document && f->vs && fn);
	if_assert_failed return 0;

	p1 = f->document->nlinks - 1;
	yl = f->vs->view_pos + f->yw;

	if (yl > f->document->y) yl = f->document->y;
	for (y = f->vs->view_pos < 0 ? 0 : f->vs->view_pos; y < yl; y++) {
		if (f->document->lines1[y] && f->document->lines1[y] - f->document->links < p1)
			p1 = f->document->lines1[y] - f->document->links;
		if (f->document->lines2[y] && f->document->lines2[y] - f->document->links > p2)
			p2 = f->document->lines2[y] - f->document->links;
	}
	/*while (p >= 0 && p < f->document->nlinks) {*/
	while (p >= p1 && p <= p2) {
		if (fn(f, &f->document->links[p])) {
			f->vs->current_link = p;
			if (cntr) cntr(f, &f->document->links[p]);
			return 1;
		}
		p += d;
	}
	f->vs->current_link = -1;
	return 0;
}


void
set_pos_x(struct document_view *f, struct link *l)
{
	int xm = 0;
	int xl = MAXINT;
	register int i;

	assert(f && l);
	if_assert_failed return;

	for (i = 0; i < l->n; i++) {
		if (l->pos[i].y >= f->vs->view_pos
		    && l->pos[i].y < f->vs->view_pos + f->yw) {
			if (l->pos[i].x >= xm) xm = l->pos[i].x + 1;
			if (l->pos[i].x < xl) xl = l->pos[i].x;
		}
	}
	if (xl == MAXINT) return;
	/*if ((f->vs->view_posx = xm - f->xw) > xl) f->vs->view_posx = xl;*/
	if (f->vs->view_posx + f->xw < xm) f->vs->view_posx = xm - f->xw;
	if (f->vs->view_posx > xl) f->vs->view_posx = xl;
}

void
set_pos_y(struct document_view *f, struct link *l)
{
	int ym = 0;
	int yl;
	register int i;

	assert(f && f->document && f->vs && l);
	if_assert_failed return;

	yl = f->document->y;
	for (i = 0; i < l->n; i++) {
		if (l->pos[i].y >= ym) ym = l->pos[i].y + 1;
		if (l->pos[i].y < yl) yl = l->pos[i].y;
	}
	f->vs->view_pos = (ym + yl) / 2 - f->document->opt.yw / 2;
	if (f->vs->view_pos > f->document->y - f->document->opt.yw)
		f->vs->view_pos = f->document->y - f->document->opt.yw;
	if (f->vs->view_pos < 0) f->vs->view_pos = 0;
}


void
find_link(struct document_view *f, int p, int s)
{
	/* p=1 - top, p=-1 - bottom, s=0 - pgdn, s=1 - down */
	struct link **line;
	struct link *link;
	int y, l;

	assert(f && f->document && f->vs);
	if_assert_failed return;

	if (p == -1) {
		line = f->document->lines2;
		if (!line) goto nolink;
		y = f->vs->view_pos + f->yw - 1;
		if (y >= f->document->y) y = f->document->y - 1;
		if (y < 0) goto nolink;
	} else {
		line = f->document->lines1;
		if (!line) goto nolink;
		y = f->vs->view_pos;
		if (y < 0) y = 0;
		if (y >= f->document->y) goto nolink;
	}

	link = NULL;
	do {
		if (line[y]
		    && (!link || (p > 0 ? line[y] < link : line[y] > link)))
			link = line[y];
		y += p;
	} while (!(y < 0
		   || y < f->vs->view_pos
		   || y >= f->vs->view_pos + f->document->opt.yw
		   || y >= f->document->y));

	if (!link) goto nolink;
	l = link - f->document->links;

	if (s == 0) {
		next_in_view(f, l, p, in_view, NULL);
		return;
	}
	f->vs->current_link = l;
	set_pos_x(f, link);
	return;

nolink:
	f->vs->current_link = -1;
}


unsigned char *
get_link_url(struct session *ses, struct document_view *f, struct link *l)
{
	assert(ses && f && l);
	if_assert_failed return NULL;

	if (l->type == L_LINK) {
		if (!l->where) return stracpy(l->where_img);
		return stracpy(l->where);
	}
	if (l->type != L_BUTTON && l->type != L_FIELD) return NULL;
	return get_form_url(ses, f, l->form);
}


/* This is common backend for submit_form_do() and enter(). */
int
goto_link(unsigned char *url, unsigned char *target, struct session *ses,
	  int do_reload)
{
	assert(url && ses);
	if_assert_failed return 1;

	/* if (strlen(url) > 4 && !strncasecmp(url, "MAP@", 4)) { */
	if (((url[0]|32) == 'm') &&
	    ((url[1]|32) == 'a') &&
	    ((url[2]|32) == 'p') &&
	    (url[3] == '@') &&
	    url[4]) {
		/* TODO: Test reload? */
		unsigned char *s = stracpy(url + 4);

		if (!s) {
			mem_free(url);
			return 1;
		}

		goto_imgmap(ses, url + 4, s,
			   target ? stracpy(target) : NULL);

	} else {
		if (do_reload) {
			goto_url_frame_reload(ses, url, target);
		} else {
			goto_url_frame(ses, url, target);
		}
	}

	mem_free(url);

	return 2;
}


int
enter(struct session *ses, struct document_view *fd, int a)
{
	struct link *link;

	assert(ses && fd && fd->vs && fd->document);
	if_assert_failed return 1;

	if (fd->vs->current_link == -1) return 1;
	link = &fd->document->links[fd->vs->current_link];

	if (link->type == L_LINK || link->type == L_BUTTON
	    || ((has_form_submit(fd->document, link->form)
		 || get_opt_int("document.browse.forms.auto_submit"))
		&& (link->type == L_FIELD || link->type == L_AREA))) {
		unsigned char *url = get_link_url(ses, fd, link);

		if (url)
			return goto_link(url, link->target, ses, a);

	} else if (link->type == L_FIELD || link->type == L_AREA) {
		/* We won't get here if (has_form_submit() ||
		 * 			 get_opt_int("..")) */
		down(ses, fd, 0);

	} else if (link->type == L_CHECKBOX) {
		struct form_state *fs = find_form_state(fd, link->form);

		if (link->form->ro)
			return 1;

		if (link->form->type == FC_CHECKBOX) {
			fs->state = !fs->state;

		} else {
			struct form_control *fc;

			foreach (fc, fd->document->forms) {
				if (fc->form_num == link->form->form_num
				    && fc->type == FC_RADIO
				    && !xstrcmp(fc->name, link->form->name)) {
					struct form_state *frm_st;

					frm_st = find_form_state(fd, fc);
					if (frm_st) frm_st->state = 0;
				}
			}
			fs->state = 1;
		}

	} else if (link->type == L_SELECT) {
		if (link->form->ro)
			return 1;

		fd->document->refcount++;
		add_empty_window(ses->tab->term,
				 (void (*)(void *)) decrement_fc_refcount,
				 fd->document);
		do_select_submenu(ses->tab->term, link->form->menu, ses);

	} else {
		internal("bad link type %d", link->type);
	}

	return 1;
}


void
selected_item(struct terminal *term, void *pitem, struct session *ses)
{
	int item = (int) pitem;
	struct document_view *f;
	struct link *l;
	struct form_state *fs;

	assert(term && ses);
	if_assert_failed return;
	f = current_frame(ses);

	assert(f && f->vs && f->document);
	if_assert_failed return;
	if (f->vs->current_link == -1) return;
	l = &f->document->links[f->vs->current_link];
	if (l->type != L_SELECT) return;

	fs = find_form_state(f, l->form);
	if (fs) {
		struct form_control *frm = l->form;

		if (item >= 0 && item < frm->nvalues) {
			fs->state = item;
			if (fs->value) mem_free(fs->value);
			fs->value = stracpy(frm->values[item]);
		}
		fixup_select_state(frm, fs);
	}

	draw_doc(ses->tab->term, f, 1);
	print_screen_status(ses);
	redraw_from_window(ses->tab);
#if 0
	if (!has_form_submit(f->document, l->form)) {
		goto_form(ses, f, l->form, l->target);
	}
#endif
}

int
get_current_state(struct session *ses)
{
	struct document_view *f;
	struct link *l;
	struct form_state *fs;

	assert(ses);
	if_assert_failed return -1;
	f = current_frame(ses);

	assert(f && f->vs && f->document);
	if_assert_failed return -1;
	if (f->vs->current_link == -1) return -1;
	l = &f->document->links[f->vs->current_link];
	if (l->type != L_SELECT) return -1;
	fs = find_form_state(f, l->form);
	if (fs) return fs->state;
	return -1;
}


struct link *
choose_mouse_link(struct document_view *f, struct event *ev)
{
	struct link *l1, *l2, *l;
	register int i;

	assert(f && f->vs && f->document && ev);
	if_assert_failed return NULL;

	l1 = f->document->links + f->document->nlinks;
	l2 = f->document->links;

	if (!f->document->nlinks
	    || ev->x < 0 || ev->y < 0 || ev->x >= f->xw || ev->y >= f->yw)
		return NULL;

	for (i = f->vs->view_pos;
	     i < f->document->y && i < f->vs->view_pos + f->yw;
	     i++) {
		if (f->document->lines1[i] && f->document->lines1[i] < l1)
			l1 = f->document->lines1[i];
		if (f->document->lines2[i] && f->document->lines2[i] > l2)
			l2 = f->document->lines2[i];
	}

	for (l = l1; l <= l2; l++) {
		for (i = 0; i < l->n; i++)
			if (l->pos[i].x - f->vs->view_posx == ev->x
			    && l->pos[i].y - f->vs->view_pos == ev->y)
				return l;
	}

	return NULL;
}


/* This is backend of the backend goto_link_number_do() below ;)). */
void
jump_to_link_number(struct session *ses, struct document_view *fd, int n)
{
	assert(ses && fd && fd->vs && fd->document);
	if_assert_failed return;

	if (n < 0 || n > fd->document->nlinks) return;
	fd->vs->current_link = n;
	check_vs(fd);
}

/* This is common backend for goto_link_number() and try_document_key(). */
static void
goto_link_number_do(struct session *ses, struct document_view *fd, int n)
{
	struct link *link;

	assert(ses && fd && fd->document);
	if_assert_failed return;
	if (n < 0 || n > fd->document->nlinks) return;
	jump_to_link_number(ses, fd, n);

	link = &fd->document->links[n];
	if (link->type != L_AREA
	    && link->type != L_FIELD
	    && get_opt_int("document.browse.accesskey.auto_follow"))
		enter(ses, fd, 0);
}

void
goto_link_number(struct session *ses, unsigned char *num)
{
	struct document_view *fd;

	assert(ses && num);
	if_assert_failed return;
	fd = current_frame(ses);
	assert(fd);
	if_assert_failed return;
	goto_link_number_do(ses, fd, atoi(num) - 1);
}

/* See if this document is interested in the key user pressed. */
int
try_document_key(struct session *ses, struct document_view *fd,
		 struct event *ev)
{
	long x;
	int passed = -1;
	int i; /* GOD I HATE C! --FF */ /* YEAH, BRAINFUCK RULEZ! --pasky */

	assert(ses && fd && fd->document && fd->vs && ev);
	if_assert_failed return 0;

	x = (ev->x < 0x100) ? upcase(ev->x) : ev->x;
	if (x >= 'A' && x <= 'Z' && ev->y != KBD_ALT) {
		/* We accept those only in alt-combo. */
		return 0;
	}

	/* Run through all the links and see if one of them is bound to the
	 * key we test.. */

	for (i = 0; i < fd->document->nlinks; i++) {
		struct link *link = &fd->document->links[i];

		if (x == link->accesskey) {
			if (passed != i && i <= fd->vs->current_link) {
				/* This is here in order to rotate between
				 * links with same accesskey. */
				if (passed < 0)	passed = i;
				continue;
			}
			goto_link_number_do(ses, fd, i);
			return 1;
		}

		if (i == fd->document->nlinks - 1 && passed >= 0) {
			/* Return to the start. */
			i = passed - 1;
		}
	}

	return 0;
}


/* Open a contextual menu on a link, form or image element. */
/* TODO: This should be completely configurable. */
void
link_menu(struct terminal *term, void *xxx, struct session *ses)
{
	struct document_view *fd;
	struct link *link;
	struct menu_item *mi;
	int l = 0;

	assert(term && ses);
	if_assert_failed return;

	fd = current_frame(ses);
	mi = new_menu(FREE_LIST);
	if (!mi) return;
	if (!fd) goto end;

	assert(fd->vs && fd->document);
	if_assert_failed return;
	if (fd->vs->current_link < 0) goto end;

	link = &fd->document->links[fd->vs->current_link];
	if (link->type == L_LINK && link->where) {
		l = 1;
		if (strlen(link->where) >= 4
		    && !strncasecmp(link->where, "MAP@", 4))
			add_to_menu(&mi, N_("Display ~usemap"), M_SUBMENU,
				    MENU_FUNC send_enter, NULL, 1, 0);
		else {
			int c = can_open_in_new(term);

			add_to_menu(&mi, N_("~Follow link"), "",
				    MENU_FUNC send_enter, NULL, 0, 0);

			add_to_menu(&mi, N_("Follow link and r~eload"), "",
				    MENU_FUNC send_enter_reload, NULL, 0, 0);

			if (c)
				add_to_menu(&mi, N_("Open in new ~window"),
					     c - 1 ? M_SUBMENU : (unsigned char *) "",
					     MENU_FUNC open_in_new_window,
					     send_open_in_new_xterm, c - 1, 0);

			if (!get_opt_int_tree(cmdline_options, "anonymous")) {
				add_to_menu(&mi, N_("~Download link"), "d",
					    MENU_FUNC send_download, NULL, 0, 0);

#ifdef BOOKMARKS
				add_to_menu(&mi, N_("~Add link to bookmarks"), "A",
					    MENU_FUNC launch_bm_add_link_dialog,
					    NULL, 0, 0);
#endif
			}

		}
	}

	if (link->form) {
		l = 1;
		if (link->form->type == FC_RESET) {
			add_to_menu(&mi, N_("~Reset form"), "",
				    MENU_FUNC send_enter, NULL, 0, 0);
		} else {
			int c = can_open_in_new(term);

			add_to_menu(&mi, N_("~Submit form"), "",
				    MENU_FUNC submit_form, NULL, 0, 0);

			add_to_menu(&mi, N_("Submit form and rel~oad"), "",
				    MENU_FUNC submit_form_reload, NULL, 0, 0);

			if (c && link->form->method == FM_GET)
				add_to_menu(&mi, N_("Submit form and open in new ~window"),
					    c - 1 ? M_SUBMENU : (unsigned char *) "",
					    MENU_FUNC open_in_new_window,
					    send_open_in_new_xterm, c - 1, 0);

			if (!get_opt_int_tree(cmdline_options, "anonymous"))
				add_to_menu(&mi, N_("Submit form and ~download"), "d",
					    MENU_FUNC send_download, NULL, 0, 0);
		}
	}

	if (link->where_img) {
		l = 1;
		add_to_menu(&mi, N_("V~iew image"), "",
			    MENU_FUNC send_image, NULL, 0, 0);
		if (!get_opt_int_tree(cmdline_options, "anonymous"))
			add_to_menu(&mi, N_("Download ima~ge"), "",
				    MENU_FUNC send_download_image, NULL, 0, 0);
	}

end:
	if (!l) {
		add_to_menu(&mi, N_("No link selected"), M_BAR,
			    NULL, NULL, 0, 0);
	}
	do_menu(term, mi, ses, 1);
}


/* Return current link's title. Pretty trivial. */
unsigned char *
print_current_link_title_do(struct document_view *fd, struct terminal *term)
{
	struct link *link;

	assert(term && fd && fd->document && fd->vs);
	if_assert_failed return NULL;

	if (fd->document->frame || fd->vs->current_link == -1
	    || fd->vs->current_link >= fd->document->nlinks)
		return NULL;

	link = &fd->document->links[fd->vs->current_link];

	if (link->title)
		return stracpy(link->title);

	return NULL;
}


unsigned char *
print_current_link_do(struct document_view *fd, struct terminal *term)
{
	struct link *link;

	assert(term && fd && fd->document && fd->vs);
	if_assert_failed return NULL;

	if (fd->document->frame || fd->vs->current_link == -1
	    || fd->vs->current_link >= fd->document->nlinks) {
		return NULL;
	}

	link = &fd->document->links[fd->vs->current_link];

	if (link->type == L_LINK) {
		struct string str;
		unsigned char *uristring;

		if (!init_string(&str)) return NULL;

		if (!link->where && link->where_img) {
			add_to_string(&str, _("Image", term));
			add_char_to_string(&str, ' ');
			uristring = link->where_img;

		} else if (strlen(link->where) >= 4
			   && !strncasecmp(link->where, "MAP@", 4)) {
			add_to_string(&str, _("Usemap", term));
			add_char_to_string(&str, ' ');
			uristring = link->where + 4;

		} else {
			uristring = link->where;
		}

		/* Add the uri with password and post info stripped */
		add_string_uri_to_string(&str, uristring,
					 ~(URI_PASSWORD | URI_POST));
		return str.source;
	}

	if (!link->form) return NULL;

	if (link->type == L_BUTTON) {
		struct string str;

		if (link->form->type == FC_RESET)
			return stracpy(_("Reset form", term));

		if (!link->form->action) return NULL;

		if (!init_string(&str)) return NULL;

		if (link->form->method == FM_GET)
			add_to_string(&str, _("Submit form to", term));
		else
			add_to_string(&str, _("Post form to", term));
		add_char_to_string(&str, ' ');

		/* Add the uri with password and post info stripped */
		add_string_uri_to_string(&str, link->form->action,
					 ~(URI_PASSWORD | URI_POST));
		return str.source;
	}

	if (link->type == L_CHECKBOX || link->type == L_SELECT
	    || link->type == L_FIELD || link->type == L_AREA) {
		struct string str;

		if (!init_string(&str)) return NULL;

		if (link->form->type == FC_RADIO)
			add_to_string(&str, _("Radio button", term));

		else if (link->form->type == FC_CHECKBOX)
			add_to_string(&str, _("Checkbox", term));

		else if (link->form->type == FC_SELECT)
			add_to_string(&str, _("Select field", term));

		else if (link->form->type == FC_TEXT)
			add_to_string(&str, _("Text field", term));

		else if (link->form->type == FC_TEXTAREA)
			add_to_string(&str, _("Text area", term));

		else if (link->form->type == FC_FILE)
			add_to_string(&str, _("File upload", term));

		else if (link->form->type == FC_PASSWORD)
			add_to_string(&str, _("Password field", term));

		else {
			done_string(&str);
			return NULL;
		}

		if (link->form->name && link->form->name[0]) {
			add_to_string(&str, ", ");
			add_to_string(&str, _("name", term));
			add_char_to_string(&str, ' ');
			add_to_string(&str, link->form->name);
		}

		if ((link->form->type == FC_CHECKBOX ||
		     link->form->type == FC_RADIO)
		    && link->form->default_value
		    && link->form->default_value[0]) {
			add_to_string(&str, ", ");
			add_to_string(&str, _("value", term));
			add_char_to_string(&str, ' ');
			add_to_string(&str, link->form->default_value);
		}

		if (link->type == L_FIELD
		    && !has_form_submit(fd->document, link->form)
		    && link->form->action) {
			add_to_string(&str, ", ");
			add_to_string(&str, _("hit ENTER to", term));
			add_char_to_string(&str, ' ');
			if (link->form->method == FM_GET)
				add_to_string(&str, _("submit to", term));
			else
				add_to_string(&str, _("post to", term));
			add_char_to_string(&str, ' ');

			/* Add the uri with password and post info stripped */
			add_string_uri_to_string(&str, link->form->action,
						 ~(URI_PASSWORD | URI_POST));
		}

		return str.source;
	}

	/* Uh-oh? */
	return NULL;
}

unsigned char *
print_current_link(struct session *ses)
{
	struct document_view *fd;

	assert(ses && ses->tab && ses->tab->term);
	if_assert_failed return NULL;
	fd = current_frame(ses);
	assert(fd);
	if_assert_failed return NULL;

	return print_current_link_do(fd, ses->tab->term);
}
