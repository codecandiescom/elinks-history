/* Links viewing/manipulation handling */
/* $Id: link.c,v 1.81 2003/10/26 23:29:36 jonas Exp $ */

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
#include "document/options.h"
#include "intl/gettext/libintl.h"
#include "protocol/uri.h"
#include "sched/session.h"
#include "terminal/color.h"
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


/* Perhaps some of these would be more fun to have in viewer/common/, dunno.
 * --pasky */

/* FIXME: Add comments!! --Zas */


void
done_link_members(struct link *link)
{
	if (link->where) mem_free(link->where);
	if (link->target) mem_free(link->target);
	if (link->title) mem_free(link->title);
	if (link->where_img) mem_free(link->where_img);
	if (link->pos) mem_free(link->pos);
	if (link->name) mem_free(link->name);
}

void
set_link(struct document_view *doc_view)
{
	assert(doc_view);
	if_assert_failed return;

	if (!c_in_view(doc_view))
		find_link(doc_view, 1, 0);
}


static int
comp_links(struct link *l1, struct link *l2)
{
	assert(l1 && l2);
	if_assert_failed return 0;
	return (l1->num - l2->num);
}

void
sort_links(struct document *document)
{
	int i;

	assert(document);
	if_assert_failed return;
	if (!document->nlinks) return;

	assert(document->links);
	if_assert_failed return;

	qsort(document->links, document->nlinks, sizeof(struct link),
	      (void *) comp_links);

	if (!document->y) return;

	document->lines1 = mem_calloc(document->y, sizeof(struct link *));
	if (!document->lines1) return;
	document->lines2 = mem_calloc(document->y, sizeof(struct link *));
	if (!document->lines2) {
		mem_free(document->lines1);
		return;
	}

	for (i = 0; i < document->nlinks; i++) {
		struct link *link = &document->links[i];
		register int p, q, j;

		if (!link->n) {
			done_link_members(link);
			memmove(link, link + 1,
				(document->nlinks - i - 1) * sizeof(struct link));
			document->nlinks--;
			i--;
			continue;
		}
		p = link->pos[0].y;
		q = link->pos[link->n - 1].y;
		if (p > q) j = p, p = q, q = j;
		for (j = p; j <= q; j++) {
			if (j >= document->y) {
				internal("link out of screen");
				continue;
			}
			document->lines2[j] = &document->links[i];
			if (!document->lines1[j])
				document->lines1[j] = &document->links[i];
		}
	}
}


static void
draw_link(struct terminal *t, struct document_view *doc_view, int l)
{
	struct link *link;
	int xmax, ymax;
	int xpos, ypos;
	int i;
	int cursor_offset = 0;
	struct screen_char *template;
	enum color_flags color_flags = COLOR_DECREASE_LIGHTNESS;
	struct color_pair colors;

	assert(t && doc_view && doc_view->vs);
	if_assert_failed return;
	assertm(!doc_view->link_bg, "link background not empty");
	if_assert_failed mem_free(doc_view->link_bg);

	if (l == -1) return;

	link = &doc_view->document->links[l];

	switch (link->type) {
		struct form_state *fs;

		case L_LINK:
		case L_SELECT:
			break;

		case L_CHECKBOX:
			cursor_offset = 1;
			break;

		case L_BUTTON:
			cursor_offset = 2;
			break;

		case L_FIELD:
			fs = find_form_state(doc_view, link->form);
			if (fs) cursor_offset = fs->state - fs->vpos;
			break;

		case L_AREA:
			fs = find_form_state(doc_view, link->form);
			if (fs) cursor_offset = area_cursor(link->form, fs);
			break;

		default:
			internal("bad link type");
			return;
	}

	/* Allocate an extra background char to work on here. */
	doc_view->link_bg = mem_alloc((1 + link->n) * sizeof(struct link_bg));
	if (!doc_view->link_bg) return;
	doc_view->link_bg_n = link->n;

	/* Setup the template char. */
	template = &doc_view->link_bg[link->n].c;
	template->attr = 0;

	if (d_opt) {
		if (!d_opt->allow_dark_on_black)
			color_flags |= COLOR_INCREASE_CONTRAST;

		if (d_opt->ensure_contrast)
			color_flags |= COLOR_ENSURE_CONTRAST;

		if (d_opt->underline_active_link)
			template->attr |= SCREEN_ATTR_UNDERLINE;

		if (d_opt->bold_active_link)
			template->attr |= SCREEN_ATTR_BOLD;
	}

	if (d_opt->color_active_link) {
		colors.foreground = d_opt->active_link_fg;
		colors.background = d_opt->active_link_bg;

	} else if (d_opt->invert_active_link
		   && link->type != L_FIELD
		   && link->type != L_AREA) {
		colors.foreground = link->color.background;
		colors.background = link->color.foreground;

	} else {
		colors.foreground = link->color.foreground;
		colors.background = link->color.background;
	}

	set_term_color(template, &colors, color_flags, d_opt->color_mode);

	xmax = doc_view->xp + doc_view->xw;
	ymax = doc_view->yp + doc_view->yw;
	xpos = doc_view->xp - doc_view->vs->view_posx;
	ypos = doc_view->yp - doc_view->vs->view_pos;

	for (i = 0; i < link->n; i++) {
		int x = link->pos[i].x + xpos;
		int y = link->pos[i].y + ypos;
		struct screen_char *co;

		if (!(x >= doc_view->xp
		      && y >= doc_view->yp
		      && x < xmax && y < ymax)) {
			doc_view->link_bg[i].x = -1;
			doc_view->link_bg[i].y = -1;
			continue;
		}

		doc_view->link_bg[i].x = x;
		doc_view->link_bg[i].y = y;

		co = get_char(t, x, y);
		copy_screen_chars(&doc_view->link_bg[i].c, co, 1);

		if (i == cursor_offset) {
			int blockable;

			if (link->type != L_FIELD && link->type != L_AREA
			    && co->color != template->color) {
				blockable = 1;
			} else {
				blockable = 0;
			}

			set_cursor(t, x, y, blockable);
			set_window_ptr(get_current_tab(t), x, y);
		}

 		template->data = co->data;
 		copy_screen_chars(co, template, 1);
	}
}


void
free_link(struct document_view *doc_view)
{
	assert(doc_view);
	if_assert_failed return;

	if (doc_view->link_bg) {
		mem_free(doc_view->link_bg);
		doc_view->link_bg = NULL;
	}

	doc_view->link_bg_n = 0;
}


void
clear_link(struct terminal *term, struct document_view *doc_view)
{
	assert(term && doc_view);
	if_assert_failed return;

	if (doc_view->link_bg) {
		struct link_bg *link_bg = doc_view->link_bg;
		int i;

		for (i = doc_view->link_bg_n - 1; i >= 0; i--) {
			struct link_bg *bgchar = &link_bg[i];

			if (bgchar->x != -1 && bgchar->y != -1) {
				struct screen_char *co;

				co = get_char(term, bgchar->x, bgchar->y);
				copy_screen_chars(co, &bgchar->c, 1);
			}
		}

		free_link(doc_view);
	}
}


void
draw_current_link(struct terminal *term, struct document_view *doc_view)
{
	assert(term && doc_view && doc_view->vs);
	if_assert_failed return;

	draw_searched(term, doc_view);
	draw_link(term, doc_view, doc_view->vs->current_link);
}


struct link *
get_first_link(struct document_view *doc_view)
{
	struct link *l;
	struct document *document;
	int width;
	register int i;

	assert(doc_view && doc_view->document);
	if_assert_failed return NULL;

	document = doc_view->document;

	if (!document->lines1) return NULL;

	l = document->links + document->nlinks;
	width = doc_view->vs->view_pos + doc_view->yw;

	for (i = doc_view->vs->view_pos; i < width; i++) {
		if (i >= 0
		    && i < document->y
		    && document->lines1[i]
		    && document->lines1[i] < l)
			l = document->lines1[i];
	}

	return (l == document->links + document->nlinks) ? NULL : l;
}

struct link *
get_last_link(struct document_view *doc_view)
{
	struct link *l = NULL;
	register int i;

	assert(doc_view && doc_view->document);
	if_assert_failed return NULL;

	if (!doc_view->document->lines2) return NULL;

	for (i = doc_view->vs->view_pos; i < doc_view->vs->view_pos + doc_view->yw; i++)
		if (i >= 0 && i < doc_view->document->y && doc_view->document->lines2[i] > l)
			l = doc_view->document->lines2[i];
	return l;
}


static int
in_viewx(struct document_view *doc_view, struct link *link)
{
	register int i;

	assert(doc_view && link);
	if_assert_failed return 0;

	for (i = 0; i < link->n; i++) {
		int x = link->pos[i].x - doc_view->vs->view_posx;

		if (x >= 0 && x < doc_view->xw)
			return 1;
	}

	return 0;
}

int
in_viewy(struct document_view *doc_view, struct link *link)
{
	register int i;

	assert(doc_view && link);
	if_assert_failed return 0;

	for (i = 0; i < link->n; i++) {
		int y = link->pos[i].y - doc_view->vs->view_pos;

		if (y >= 0 && y < doc_view->yw)
			return 1;
	}

	return 0;
}

int
in_view(struct document_view *doc_view, struct link *link)
{
	assert(doc_view && link);
	if_assert_failed return 0;
	return in_viewy(doc_view, link) && in_viewx(doc_view, link);
}

int
c_in_view(struct document_view *doc_view)
{
	assert(doc_view && doc_view->vs);
	if_assert_failed return 0;
	return (doc_view->vs->current_link != -1
		&& in_view(doc_view, &doc_view->document->links[doc_view->vs->current_link]));
}

int
next_in_view(struct document_view *doc_view, int p, int d,
	     int (*fn)(struct document_view *, struct link *),
	     void (*cntr)(struct document_view *, struct link *))
{
	int p1, p2 = 0;
	int y, yl;

	assert(doc_view && doc_view->document && doc_view->vs && fn);
	if_assert_failed return 0;

	p1 = doc_view->document->nlinks - 1;
	yl = doc_view->vs->view_pos + doc_view->yw;

	if (yl > doc_view->document->y) yl = doc_view->document->y;
	for (y = int_max(0, doc_view->vs->view_pos); y < yl; y++) {
		if (doc_view->document->lines1[y]
		    && doc_view->document->lines1[y] - doc_view->document->links < p1)
			p1 = doc_view->document->lines1[y] - doc_view->document->links;
		if (doc_view->document->lines2[y]
		    && doc_view->document->lines2[y] - doc_view->document->links > p2)
			p2 = doc_view->document->lines2[y] - doc_view->document->links;
	}

	while (p >= p1 && p <= p2) {
		if (fn(doc_view, &doc_view->document->links[p])) {
			doc_view->vs->current_link = p;
			if (cntr) cntr(doc_view, &doc_view->document->links[p]);
			return 1;
		}
		p += d;
	}

	doc_view->vs->current_link = -1;
	return 0;
}


void
set_pos_x(struct document_view *doc_view, struct link *link)
{
	int xm = 0;
	int xl = MAXINT;
	register int i;

	assert(doc_view && link);
	if_assert_failed return;

	for (i = 0; i < link->n; i++) {
		if (link->pos[i].y >= doc_view->vs->view_pos
		    && link->pos[i].y < doc_view->vs->view_pos + doc_view->yw) {
			/* XXX: bug ?? if l->pos[i].x == xm => xm = xm + 1 --Zas*/
			if (link->pos[i].x >= xm) xm = link->pos[i].x + 1;
			xl = int_min(xl, link->pos[i].x);
		}
	}
	if (xl == MAXINT) return;
	/* if ((doc_view->vs->view_posx = xm - doc_view->xw) > xl)
	 *	doc_view->vs->view_posx = xl;*/
	int_bounds(&doc_view->vs->view_posx, xm - doc_view->xw, xl);
}

void
set_pos_y(struct document_view *doc_view, struct link *link)
{
	int ym = 0;
	int yl;
	register int i;

	assert(doc_view && doc_view->document && doc_view->vs && link);
	if_assert_failed return;

	yl = doc_view->document->y;
	for (i = 0; i < link->n; i++) {
		if (link->pos[i].y >= ym) ym = link->pos[i].y + 1;
		yl = int_min(yl, link->pos[i].y);
	}
	doc_view->vs->view_pos = (ym + yl) / 2 - doc_view->document->opt.yw / 2;
	int_bounds(&doc_view->vs->view_pos, 0,
		   doc_view->document->y - doc_view->document->opt.yw);
}

void
find_link(struct document_view *doc_view, int p, int s)
{
	/* p=1 - top, p=-1 - bottom, s=0 - pgdn, s=1 - down */
	struct link **line;
	struct link *link;
	int y, l;

	assert(doc_view && doc_view->document && doc_view->vs);
	if_assert_failed return;

	if (p == -1) {
		line = doc_view->document->lines2;
		if (!line) goto nolink;
		y = doc_view->vs->view_pos + doc_view->yw - 1;
		if (y >= doc_view->document->y) y = doc_view->document->y - 1;
		if (y < 0) goto nolink;
	} else {
		line = doc_view->document->lines1;
		if (!line) goto nolink;
		y = doc_view->vs->view_pos;
		if (y < 0) y = 0;
		if (y >= doc_view->document->y) goto nolink;
	}

	link = NULL;
	do {
		if (line[y]
		    && (!link || (p > 0 ? line[y] < link : line[y] > link)))
			link = line[y];
		y += p;
	} while (!(y < 0
		   || y < doc_view->vs->view_pos
		   || y >= doc_view->vs->view_pos + doc_view->document->opt.yw
		   || y >= doc_view->document->y));

	if (!link) goto nolink;
	l = link - doc_view->document->links;

	if (s == 0) {
		next_in_view(doc_view, l, p, in_view, NULL);
		return;
	}
	doc_view->vs->current_link = l;
	set_pos_x(doc_view, link);
	return;

nolink:
	doc_view->vs->current_link = -1;
}


unsigned char *
get_link_url(struct session *ses, struct document_view *doc_view, struct link *l)
{
	assert(ses && doc_view && l);
	if_assert_failed return NULL;

	if (l->type == L_LINK) {
		if (!l->where) return stracpy(l->where_img);
		return stracpy(l->where);
	}
	if (l->type != L_BUTTON && l->type != L_FIELD) return NULL;
	return get_form_url(ses, doc_view, l->form);
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


static void
decrement_document_refcount(struct document *document)
{
	assert(document);
	if_assert_failed return;

	if (!--document->refcount) format_cache_entries++;
	assertm(document->refcount >= 0, "reference count underflow");
	if_assert_failed document->refcount = 0;
}

int
enter(struct session *ses, struct document_view *doc_view, int a)
{
	struct link *link;

	assert(ses && doc_view && doc_view->vs && doc_view->document);
	if_assert_failed return 1;

	if (doc_view->vs->current_link == -1) return 1;
	link = &doc_view->document->links[doc_view->vs->current_link];

	if (link->type == L_LINK || link->type == L_BUTTON
	    || ((has_form_submit(doc_view->document, link->form)
		 || get_opt_int("document.browse.forms.auto_submit"))
		&& (link->type == L_FIELD || link->type == L_AREA))) {
		unsigned char *url = get_link_url(ses, doc_view, link);

		if (url)
			return goto_link(url, link->target, ses, a);

	} else if (link->type == L_FIELD || link->type == L_AREA) {
		/* We won't get here if (has_form_submit() ||
		 * 			 get_opt_int("..")) */
		down(ses, doc_view, 0);

	} else if (link->type == L_CHECKBOX) {
		struct form_state *fs = find_form_state(doc_view, link->form);

		if (link->form->ro) return 1;

		if (link->form->type == FC_CHECKBOX) {
			fs->state = !fs->state;

		} else {
			struct form_control *fc;

			foreach (fc, doc_view->document->forms) {
				if (fc->form_num == link->form->form_num
				    && fc->type == FC_RADIO
				    && !xstrcmp(fc->name, link->form->name)) {
					struct form_state *frm_st;

					frm_st = find_form_state(doc_view, fc);
					if (frm_st) frm_st->state = 0;
				}
			}
			fs->state = 1;
		}

	} else if (link->type == L_SELECT) {
		if (link->form->ro)
			return 1;

		doc_view->document->refcount++;
		add_empty_window(ses->tab->term,
				 (void (*)(void *)) decrement_document_refcount,
				 doc_view->document);
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
	struct document_view *doc_view;
	struct link *l;
	struct form_state *fs;

	assert(term && ses);
	if_assert_failed return;
	doc_view = current_frame(ses);

	assert(doc_view && doc_view->vs && doc_view->document);
	if_assert_failed return;
	if (doc_view->vs->current_link == -1) return;
	l = &doc_view->document->links[doc_view->vs->current_link];
	if (l->type != L_SELECT) return;

	fs = find_form_state(doc_view, l->form);
	if (fs) {
		struct form_control *frm = l->form;

		if (item >= 0 && item < frm->nvalues) {
			fs->state = item;
			if (fs->value) mem_free(fs->value);
			fs->value = stracpy(frm->values[item]);
		}
		fixup_select_state(frm, fs);
	}

	draw_doc(ses->tab->term, doc_view, 1);
	print_screen_status(ses);
	redraw_from_window(ses->tab);
#if 0
	if (!has_form_submit(doc_view->document, l->form)) {
		goto_form(ses, doc_view, l->form, l->target);
	}
#endif
}

int
get_current_state(struct session *ses)
{
	struct document_view *doc_view;
	struct link *l;
	struct form_state *fs;

	assert(ses);
	if_assert_failed return -1;
	doc_view = current_frame(ses);

	assert(doc_view && doc_view->vs && doc_view->document);
	if_assert_failed return -1;
	if (doc_view->vs->current_link == -1) return -1;
	l = &doc_view->document->links[doc_view->vs->current_link];
	if (l->type != L_SELECT) return -1;
	fs = find_form_state(doc_view, l->form);
	if (fs) return fs->state;
	return -1;
}


struct link *
choose_mouse_link(struct document_view *doc_view, struct term_event *ev)
{
	struct link *l1, *l2, *l;
	register int i;

	assert(doc_view && doc_view->vs && doc_view->document && ev);
	if_assert_failed return NULL;

	l1 = doc_view->document->links + doc_view->document->nlinks;
	l2 = doc_view->document->links;

	if (!doc_view->document->nlinks
	    || ev->x < 0 || ev->y < 0
	    || ev->x >= doc_view->xw || ev->y >= doc_view->yw)
		return NULL;

	for (i = doc_view->vs->view_pos;
	     i < doc_view->document->y && i < doc_view->vs->view_pos + doc_view->yw;
	     i++) {
		if (doc_view->document->lines1[i] && doc_view->document->lines1[i] < l1)
			l1 = doc_view->document->lines1[i];
		if (doc_view->document->lines2[i] && doc_view->document->lines2[i] > l2)
			l2 = doc_view->document->lines2[i];
	}

	for (l = l1; l <= l2; l++) {
		for (i = 0; i < l->n; i++)
			if (l->pos[i].x - doc_view->vs->view_posx == ev->x
			    && l->pos[i].y - doc_view->vs->view_pos == ev->y)
				return l;
	}

	return NULL;
}


/* This is backend of the backend goto_link_number_do() below ;)). */
void
jump_to_link_number(struct session *ses, struct document_view *doc_view, int n)
{
	assert(ses && doc_view && doc_view->vs && doc_view->document);
	if_assert_failed return;

	if (n < 0 || n > doc_view->document->nlinks) return;
	doc_view->vs->current_link = n;
	check_vs(doc_view);
}

/* This is common backend for goto_link_number() and try_document_key(). */
static void
goto_link_number_do(struct session *ses, struct document_view *doc_view, int n)
{
	struct link *link;

	assert(ses && doc_view && doc_view->document);
	if_assert_failed return;
	if (n < 0 || n > doc_view->document->nlinks) return;
	jump_to_link_number(ses, doc_view, n);

	link = &doc_view->document->links[n];
	if (link->type != L_AREA
	    && link->type != L_FIELD
	    && get_opt_int("document.browse.accesskey.auto_follow"))
		enter(ses, doc_view, 0);
}

void
goto_link_number(struct session *ses, unsigned char *num)
{
	struct document_view *doc_view;

	assert(ses && num);
	if_assert_failed return;
	doc_view = current_frame(ses);
	assert(doc_view);
	if_assert_failed return;
	goto_link_number_do(ses, doc_view, atoi(num) - 1);
}

/* See if this document is interested in the key user pressed. */
int
try_document_key(struct session *ses, struct document_view *doc_view,
		 struct term_event *ev)
{
	long x;
	int passed = -1;
	int i; /* GOD I HATE C! --FF */ /* YEAH, BRAINFUCK RULEZ! --pasky */

	assert(ses && doc_view && doc_view->document && doc_view->vs && ev);
	if_assert_failed return 0;

	x = (ev->x < 0x100) ? upcase(ev->x) : ev->x;
	if (x >= 'A' && x <= 'Z' && ev->y != KBD_ALT) {
		/* We accept those only in alt-combo. */
		return 0;
	}

	/* Run through all the links and see if one of them is bound to the
	 * key we test.. */

	for (i = 0; i < doc_view->document->nlinks; i++) {
		struct link *link = &doc_view->document->links[i];

		if (x == link->accesskey) {
			if (passed != i && i <= doc_view->vs->current_link) {
				/* This is here in order to rotate between
				 * links with same accesskey. */
				if (passed < 0)	passed = i;
				continue;
			}
			goto_link_number_do(ses, doc_view, i);
			return 1;
		}

		if (i == doc_view->document->nlinks - 1 && passed >= 0) {
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
	struct document_view *doc_view;
	struct link *link;
	struct menu_item *mi;

	assert(term && ses);
	if_assert_failed return;

	doc_view = current_frame(ses);
	mi = new_menu(FREE_LIST);
	if (!mi) return;
	if (!doc_view) goto end;

	assert(doc_view->vs && doc_view->document);
	if_assert_failed return;
	if (doc_view->vs->current_link < 0) goto end;

	link = &doc_view->document->links[doc_view->vs->current_link];
	if (link->type == L_LINK && link->where) {
		if (strlen(link->where) >= 4
		    && !strncasecmp(link->where, "MAP@", 4))
			add_to_menu(&mi, N_("Display ~usemap"), M_SUBMENU,
				    (menu_func) send_enter, NULL, 1, 0);
		else {
			int c = can_open_in_new(term);

			add_to_menu(&mi, N_("~Follow link"), "",
				    (menu_func) send_enter, NULL, 0, 0);

			add_to_menu(&mi, N_("Follow link and r~eload"), "",
				    (menu_func) send_enter_reload, NULL, 0, 0);

			if (c)
				add_to_menu(&mi, N_("Open in new ~window"),
					     c - 1 ? M_SUBMENU : (unsigned char *) "",
					     (menu_func) open_in_new_window,
					     send_open_in_new_window, c - 1, 0);

			add_to_menu(&mi, N_("Open in new ~tab"), "",
				     (menu_func) open_in_new_tab, (void *) 1, 0, 0);

			add_to_menu(&mi, N_("Open in new tab in ~background"), "",
				     (menu_func) open_in_new_tab_in_background, (void *) 1, 0, 0);

			if (!get_opt_int_tree(cmdline_options, "anonymous")) {
				add_to_menu(&mi, N_("~Download link"), "d",
					    (menu_func) send_download, NULL, 0, 0);

#ifdef BOOKMARKS
				add_to_menu(&mi, N_("~Add link to bookmarks"), "A",
					    (menu_func) launch_bm_add_link_dialog,
					    NULL, 0, 0);
#endif
			}

		}
	}

	if (link->form) {
		if (link->form->type == FC_RESET) {
			add_to_menu(&mi, N_("~Reset form"), "",
				    (menu_func) send_enter, NULL, 0, 0);
		} else {
			int c = can_open_in_new(term);

			add_to_menu(&mi, N_("~Submit form"), "",
				    (menu_func) submit_form, NULL, 0, 0);

			add_to_menu(&mi, N_("Submit form and rel~oad"), "",
				    (menu_func) submit_form_reload, NULL, 0, 0);

			if (c && link->form->method == FM_GET)
				add_to_menu(&mi, N_("Submit form and open in new ~window"),
					    c - 1 ? M_SUBMENU : (unsigned char *) "",
					    (menu_func) open_in_new_window,
					    send_open_in_new_window, c - 1, 0);

			if (!get_opt_int_tree(cmdline_options, "anonymous"))
				add_to_menu(&mi, N_("Submit form and ~download"), "d",
					    (menu_func) send_download, NULL, 0, 0);
		}
	}

	if (link->where_img) {
		add_to_menu(&mi, N_("V~iew image"), "",
			    (menu_func) send_image, NULL, 0, 0);
		if (!get_opt_int_tree(cmdline_options, "anonymous"))
			add_to_menu(&mi, N_("Download ima~ge"), "",
				    (menu_func) send_download_image, NULL, 0, 0);
	}

end:
	if (!mi->text) {
		add_to_menu(&mi, N_("No link selected"), M_BAR,
			    NULL, NULL, 0, 0);
	}

	do_menu(term, mi, ses, 1);
}


/* Return current link's title. Pretty trivial. */
unsigned char *
print_current_link_title_do(struct document_view *doc_view, struct terminal *term)
{
	struct link *link;

	assert(term && doc_view && doc_view->document && doc_view->vs);
	if_assert_failed return NULL;

	if (doc_view->document->frame_desc || doc_view->vs->current_link == -1
	    || doc_view->vs->current_link >= doc_view->document->nlinks)
		return NULL;

	link = &doc_view->document->links[doc_view->vs->current_link];

	if (link->title)
		return stracpy(link->title);

	return NULL;
}


unsigned char *
print_current_link_do(struct document_view *doc_view, struct terminal *term)
{
	struct link *link;

	assert(term && doc_view && doc_view->document && doc_view->vs);
	if_assert_failed return NULL;

	if (doc_view->document->frame_desc
	    || doc_view->vs->current_link == -1
	    || doc_view->vs->current_link >= doc_view->document->nlinks)
		return NULL;

	link = &doc_view->document->links[doc_view->vs->current_link];

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
		    && !has_form_submit(doc_view->document, link->form)
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
	struct document_view *doc_view;

	assert(ses && ses->tab && ses->tab->term);
	if_assert_failed return NULL;
	doc_view = current_frame(ses);
	assert(doc_view);
	if_assert_failed return NULL;

	return print_current_link_do(doc_view, ses->tab->term);
}
