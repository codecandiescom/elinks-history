/* HTML viewer (and much more) */
/* $Id: view.c,v 1.146 2003/07/02 23:56:09 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h> /* OS/2 needs this after sys/types.h */
#endif
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
#include "protocol/url.h"
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
#include "viewer/text/search.h"
#include "viewer/text/view.h"
#include "viewer/text/vs.h"


/* FIXME: Add comments!! --Zas */
/* TODO: This file needs to be splitted to many smaller ones. Definitively.
 * --pasky */

void
init_formatted(struct f_data *scr)
{
	struct list_head tmp;

	memcpy(&tmp, (struct f_data **)scr, sizeof(struct list_head));
	memset(((struct f_data **)scr), 0, sizeof(struct f_data));
	memcpy((struct f_data **)scr, &tmp, sizeof(struct list_head));

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
clear_formatted(struct f_data *scr)
{
	int n;
	int y;
	struct cache_entry *ce;
	struct form_control *fc;

	assert(scr);

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
destroy_formatted(struct f_data *scr)
{
	assert(scr);
	assertm(!scr->refcount, "Attempt to free locked formatted data.");

	clear_formatted(scr);
	del_from_list(scr);
	mem_free(scr);
}

void
detach_formatted(struct f_data_c *scr)
{
	assert(scr);

	if (scr->f_data) {
		format_cache_reactivate(scr->f_data);
		if (!--scr->f_data->refcount) {
			format_cache_entries++;
			/*shrink_format_cache();*/
		}
		assertm(scr->f_data->refcount >= 0,
			"format_cache refcount underflow");
		scr->f_data = NULL;
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

void
set_link(struct f_data_c *f)
{
	if (c_in_view(f)) return;
	find_link(f, 1, 0);
}

static inline int
find_tag(struct f_data *f, unsigned char *name)
{
	struct tag *tag;

	foreach (tag, f->tags)
		if (!strcasecmp(tag->name, name))
			return tag->y;

	return -1;
}

static int
comp_links(struct link *l1, struct link *l2)
{
	return (l1->num - l2->num);
}

void
sort_links(struct f_data *f)
{
	int i;

	assert(f);
	if (!f->nlinks) return;

	assert(f->links);

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
		int p, q, j;
		struct link *link = &f->links[i];

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

struct form_state *find_form_state(struct f_data_c *, struct form_control *);

struct line_info {
	unsigned char *st;
	unsigned char *en;
};

static struct line_info *
format_text(unsigned char *text, int width, int wrap)
{
	struct line_info *ln = NULL;
	int lnn = 0;
	unsigned char *b = text;
	int sk, ps = 0;

	assert(text);

	while (*text) {
		unsigned char *s;

		if (*text == '\n') {
			sk = 1;

put:
			if (!(lnn & (ALLOC_GR - 1))) {
				struct line_info *_ln = mem_realloc(ln,
						        (lnn + ALLOC_GR)
							* sizeof(struct line_info));

				if (!_ln) {
					if (ln) mem_free(ln);
					return NULL;
				}
				ln = _ln;
			}
			ln[lnn].st = b;
			ln[lnn++].en = text;
			b = text += sk;
			continue;
		}
		if (!wrap || text - b < width) {
			text++;
			continue;
		}
		for (s = text; s >= b; s--) if (*s == ' ') {
			if (wrap == 2) *s = '\n';
			text = s;
			sk = 1;
			goto put;
		}
		sk = 0;
		goto put;
	}

	if (ps < 2) {
		ps++;
		sk = 0;
		goto put;
	}
	ln[lnn - 1].st = ln[lnn - 1].en = NULL;

	return ln;
}

static int
area_cursor(struct form_control *frm, struct form_state *fs)
{
	struct line_info *ln;
	int q = 0;
	int y;

	assert(frm && fs);

	ln = format_text(fs->value, frm->cols, !!frm->wrap);
	if (!ln) return 0;

	for (y = 0; ln[y].st; y++) {
		int x = fs->value + fs->state - ln[y].st;

		if (fs->value + fs->state < ln[y].st ||
		    fs->value + fs->state >= ln[y].en + (ln[y + 1].st != ln[y].en))
			continue;

		if (frm->wrap && x == frm->cols) x--;
		if (x >= frm->cols + fs->vpos) fs->vpos = x - frm->cols + 1;
		if (x < fs->vpos) fs->vpos = x;
		if (y >= frm->rows + fs->vypos) fs->vypos = y - frm->rows + 1;
		if (y < fs->vypos) fs->vypos = y;
		x -= fs->vpos;
		y -= fs->vypos;
		q = y * frm->cols + x;
		break;
	}
	mem_free(ln);

	return q;
}

static void
draw_link(struct terminal *t, struct f_data_c *scr, int l)
{
	struct link *link;
	struct view_state *vs;
	int xp, yp;
	int xw, yw;
	int vx, vy;
	int f = 0;

	assert(t && scr && scr->vs);
	assertm(!scr->link_bg, "link background not empty");

	if (l == -1) return;

	link = &scr->f_data->links[l];
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
						int xx = x, yy = y;

						if (link->type != L_FIELD && link->type != L_AREA) {
							if (((co >> 8) & 0x38) != (link->sel_color & 0x38)) {
							       xx = xp + xw - 1;
							       yy = yp + yw - 1;
							}
						}
						set_cursor(t, x, y, xx, yy);
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

static inline void
free_link(struct f_data_c *scr)
{
	assert(scr);

	if (scr->link_bg) mem_free(scr->link_bg), scr->link_bg = NULL;
	scr->link_bg_n = 0;
}

static void
clear_link(struct terminal *t, struct f_data_c *scr)
{
	assert(t && scr);

	if (scr->link_bg) {
		int i;

		for (i = scr->link_bg_n - 1; i >= 0; i--)
			set_char(t, scr->link_bg[i].x, scr->link_bg[i].y,
				 scr->link_bg[i].c);
		free_link(scr);
	}
}

static inline void
draw_current_link(struct terminal *t, struct f_data_c *scr)
{
	assert(t && scr && scr->vs);

	draw_link(t, scr, scr->vs->current_link);
	draw_searched(t, scr);
}

static struct link *
get_first_link(struct f_data_c *f)
{
	struct link *l;
	register int i;

	assert(f && f->f_data);

	if (!f->f_data->lines1) return NULL;

	l = f->f_data->links + f->f_data->nlinks;

	for (i = f->vs->view_pos; i < f->vs->view_pos + f->yw; i++)
		if (i >= 0 && i < f->f_data->y && f->f_data->lines1[i]
		    && f->f_data->lines1[i] < l)
			l = f->f_data->lines1[i];

	if (l == f->f_data->links + f->f_data->nlinks) l = NULL;
	return l;
}

static struct link *
get_last_link(struct f_data_c *f)
{
	struct link *l = NULL;
	register int i;

	assert(f && f->f_data);

	if (!f->f_data->lines2) return NULL;

	for (i = f->vs->view_pos; i < f->vs->view_pos + f->yw; i++)
		if (i >= 0 && i < f->f_data->y && f->f_data->lines2[i] > l)
			l = f->f_data->lines2[i];
	return l;
}

static void
fixup_select_state(struct form_control *fc, struct form_state *fs)
{
	register int i = 0;

	assert(fc && fs);

	if (fs->state >= 0
	    && fs->state < fc->nvalues
	    && !strcmp(fc->values[fs->state], fs->value))
		return;

	while (i < fc->nvalues) {
		if (!strcmp(fc->values[i], fs->value)) {
			fs->state = i;
			return;
		}
		i++;
	}

	fs->state = 0;

	if (fs->value) mem_free(fs->value);
	fs->value = stracpy(fc->nvalues ? fc->values[0] : (unsigned char *) "");
}

static void
init_ctrl(struct form_control *frm, struct form_state *fs)
{
	assert(frm && fs);

	if (fs->value) mem_free(fs->value), fs->value = NULL;

	switch (frm->type) {
		case FC_TEXT:
		case FC_PASSWORD:
		case FC_TEXTAREA:
			fs->value = stracpy(frm->default_value);
			fs->state = strlen(frm->default_value);
			fs->vpos = 0;
			break;
		case FC_FILE:
			fs->value = stracpy("");
			fs->state = 0;
			fs->vpos = 0;
			break;
		case FC_CHECKBOX:
		case FC_RADIO:
			fs->state = frm->default_state;
			break;
		case FC_SELECT:
			fs->value = stracpy(frm->default_value);
			fs->state = frm->default_state;
			fixup_select_state(frm, fs);
			break;
		case FC_SUBMIT:
		case FC_IMAGE:
		case FC_RESET:
		case FC_HIDDEN:
			/* Silence compiler warnings. */
			break;
		default:
			internal("unknown form field type");
	}
}

struct form_state *
find_form_state(struct f_data_c *f, struct form_control *frm)
{
	struct view_state *vs;
	struct form_state *fs;
	int n;

	assert(f && f->vs && frm);

	vs = f->vs;
	n = frm->g_ctrl_num;

	if (n < vs->form_info_len) fs = &vs->form_info[n];
	else {
		fs = mem_realloc(vs->form_info, (n + 1) * sizeof(struct form_state));
		if (!fs) return NULL;
		vs->form_info = fs;
		memset(fs + vs->form_info_len, 0,
		       (n + 1 - vs->form_info_len) * sizeof(struct form_state));
		vs->form_info_len = n + 1;
		fs = &vs->form_info[n];
	}

	if (fs->form_num == frm->form_num
	    && fs->ctrl_num == frm->ctrl_num
	    && fs->g_ctrl_num == frm->g_ctrl_num
	    && fs->position == frm->position
	    && fs->type == frm->type)
		return fs;

	if (fs->value) mem_free(fs->value);
	memset(fs, 0, sizeof(struct form_state));
	fs->form_num = frm->form_num;
	fs->ctrl_num = frm->ctrl_num;
	fs->g_ctrl_num = frm->g_ctrl_num;
	fs->position = frm->position;
	fs->type = frm->type;
	init_ctrl(frm, fs);

	return fs;
}


static void
draw_textarea(struct terminal *t, struct form_state *fs,
	      struct f_data_c *f, struct link *l)
{
	struct line_info *ln, *lnx;
	int sl, ye;
	register int i, x, y = 0;
	struct form_control *frm;
	int xp, yp;
	int xw, yw;
	int vx, vy;

	assert(t && f && f->f_data && l);
	frm = l->form;
	assertm(frm, "link %d has no form", (int)(l - f->f_data->links));

	xp = f->xp;
	yp = f->yp;
	xw = f->xw;
	yw = f->yw;
	vx = f->vs->view_posx;
	vy = f->vs->view_pos;


	if (!l->n) return;
	area_cursor(frm, fs);
	lnx = format_text(fs->value, frm->cols, !!frm->wrap);
	if (!lnx) return;
	ln = lnx;
	sl = fs->vypos;
	while (ln->st && sl) sl--, ln++;

	x = l->pos[0].x + xp - vx;
	y = l->pos[0].y + yp - vy;
	ye = y + frm->rows;

	for (; ln->st && y < ye; ln++, y++) {
		if (y < yp || y >= yp + yw) continue;

		for (i = 0; i < frm->cols; i++) {
			int xi = x + i;

			if (xi >= xp && xi < xp + xw) {
				if (fs->value &&
				    i >= -fs->vpos &&
				    i + fs->vpos < ln->en - ln->st)
					set_only_char(t, xi, y,
						      ln->st[i + fs->vpos]);
				else
					set_only_char(t, xi, y, '_');
			}
		}
	}

	for (; y < ye; y++) {
		if (y < yp || y >= yp + yw) continue;

		for (i = 0; i < frm->cols; i++) {
			int xi = x + i;

			if (xi >= xp && xi < xp + xw)
				set_only_char(t, xi, y, '_');
		}
	}

	mem_free(lnx);
}

static void
draw_form_entry(struct terminal *t, struct f_data_c *f, struct link *l)
{
	struct form_state *fs;
	struct form_control *frm;
	struct view_state *vs;
	int xp, yp;
	int xw, yw;
	int vx, vy;

	assert(t && f && f->f_data && l);
	frm = l->form;
	assertm(frm, "link %d has no form", (int)(l - f->f_data->links));

	fs = find_form_state(f, frm);
	if (!fs) return;

	xp = f->xp;
	yp = f->yp;
	xw = f->xw;
	yw = f->yw;
	vs = f->vs;
	vx = vs->view_posx;
	vy = vs->view_pos;

	switch (frm->type) {
		unsigned char *s;
		int sl;
		register int i, x, y;

		case FC_TEXT:
		case FC_PASSWORD:
		case FC_FILE:
			if (fs->state >= fs->vpos + frm->size)
				fs->vpos = fs->state - frm->size + 1;
			if (fs->state < fs->vpos)
				fs->vpos = fs->state;
			if (!l->n) break;

			y = l->pos[0].y + yp - vy;
			if (y >= yp && y < yp + yw) {
				int len = strlen(fs->value) - fs->vpos;

				x = l->pos[0].x + xp - vx;
				for (i = 0; i < frm->size; i++, x++) {
					if (x >= xp && x < xp + xw) {
						if (fs->value &&
						    i >= -fs->vpos && i < len)
							set_only_char(t, x, y,
								      frm->type != FC_PASSWORD
								      ? fs->value[i + fs->vpos]
								      : '*');
						else
							set_only_char(t, x, y, '_');
					}
				}
			}
			break;
		case FC_TEXTAREA:
			draw_textarea(t, fs, f, l);
			break;
		case FC_CHECKBOX:
		case FC_RADIO:
			if (l->n < 2) break;
			x = l->pos[1].x + xp - vx;
			y = l->pos[1].y + yp - vy;
			if (x >= xp && y >= yp && x < xp + xw && y < yp + yw)
				set_only_char(t, x, y, fs->state ? 'X' : ' ');
			break;
		case FC_SELECT:
			fixup_select_state(frm, fs);
			if (fs->state < frm->nvalues)
				s = frm->labels[fs->state];
			else
				/* XXX: when can this happen? --pasky */
				s = "";
			sl = s ? strlen(s) : 0;
			for (i = 0; i < l->n; i++) {
				x = l->pos[i].x + xp - vx;
				y = l->pos[i].y + yp - vy;
				if (x >= xp && y >= yp && x < xp + xw && y < yp + yw)
					set_only_char(t, x, y, i < sl ? s[i] : '_');
			}
			break;
		case FC_SUBMIT:
		case FC_IMAGE:
		case FC_RESET:
		case FC_HIDDEN:
			break;
		default:
			internal("Unknown form field type.");
	}
}

static void
draw_forms(struct terminal *t, struct f_data_c *f)
{
	struct link *l1, *l2;

	assert(t && f);

	l1 = get_first_link(f);
	l2 = get_last_link(f);

	if (!l1 || !l2) {
		assertm(!l1 && !l2, "get_first_link == %p, get_last_link == %p", l1, l2);
		return;
	}
	do {
		if (l1->type != L_LINK)
			draw_form_entry(t, f, l1);
	} while (l1++ < l2);
}


unsigned char fr_trans[2][4] = {{0xb3, 0xc3, 0xb4, 0xc5}, {0xc4, 0xc2, 0xc1, 0xc5}};

/* 0 -> 1 <- 2 v 3 ^ */
enum xchar_dir {
	XD_RIGHT = 0,
	XD_LEFT,
	XD_DOWN,
	XD_UP
};

static void
set_xchar(struct terminal *t, int x, int y, enum xchar_dir dir)
{
       unsigned int c, d;

       assert(t);

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

static void
draw_doc(struct terminal *t, struct f_data_c *scr, int active)
{
	struct view_state *vs;
	int xp, yp;
	int xw, yw;
	int vx, vy;
	int y;

	assert(t && scr);

	xp = scr->xp;
	yp = scr->yp;
	xw = scr->xw;
	yw = scr->yw;

	if (active) {
		set_cursor(t, xp + xw - 1, yp + yw - 1, xp + xw - 1, yp + yw - 1);
		set_window_ptr(get_current_tab(t), xp, yp);
	}
	if (!scr->vs) {
		fill_area(t, xp, yp, xw, yw, scr->f_data->y ? scr->f_data->bg : ' ');
		return;
	}
	if (scr->f_data->frame) {
	 	fill_area(t, xp, yp, xw, yw, scr->f_data->y ? scr->f_data->bg : ' ');
		draw_frame_lines(t, scr->f_data->frame_desc, xp, yp);
		if (scr->vs && scr->vs->current_link == -1) scr->vs->current_link = 0;
		return;
	}
	check_vs(scr);
	vs = scr->vs;
	if (vs->goto_position) {
		vy = find_tag(scr->f_data, vs->goto_position);
	       	if (vy != -1) {
			if (vy > scr->f_data->y) vy = scr->f_data->y - 1;
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
	fill_area(t, xp, yp, xw, yw, scr->f_data->y ? scr->f_data->bg : ' ');
	if (!scr->f_data->y) return;

	while (vs->view_pos >= scr->f_data->y) vs->view_pos -= yw;
	if (vs->view_pos < 0) vs->view_pos = 0;
	if (vy != vs->view_pos) vy = vs->view_pos, check_vs(scr);
	for (y = vy <= 0 ? 0 : vy; y < (-vy + scr->f_data->y <= yw ? scr->f_data->y : yw + vy); y++) {
		int st = vx <= 0 ? 0 : vx;
		int en = -vx + scr->f_data->data[y].l <= xw ? scr->f_data->data[y].l : xw + vx;

		set_line(t, xp + st - vx, yp + y - vy, en - st, &scr->f_data->data[y].d[st]);
	}
	draw_forms(t, scr);
	if (active) draw_current_link(t, scr);
	if (scr->search_word && *scr->search_word && (*scr->search_word)[0]) scr->xl = scr->yl = -1;
}

static void
draw_frames(struct session *ses)
{
	struct f_data_c *f, *cf;
	int *l;
	int n, i, d, more;

	assert(ses && ses->screen && ses->screen->f_data);

	if (!ses->screen->f_data->frame) return;
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

	if (ses->tab != get_current_tab(ses->tab->term))
		return;

	if (!ses->screen || !ses->screen->f_data) {
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

static int
in_viewx(struct f_data_c *f, struct link *l)
{
	register int i;

	assert(f && l);

	for (i = 0; i < l->n; i++) {
		if (l->pos[i].x >= f->vs->view_posx
		    && l->pos[i].x < f->vs->view_posx + f->xw)
			return 1;
	}
	return 0;
}

static int
in_viewy(struct f_data_c *f, struct link *l)
{
	register int i;

	assert(f && l);

	for (i = 0; i < l->n; i++) {
		if (l->pos[i].y >= f->vs->view_pos
		    && l->pos[i].y < f->vs->view_pos + f->yw)
			return 1;
	}
	return 0;
}

int
in_view(struct f_data_c *f, struct link *l)
{
	assert(f && l);
	return in_viewy(f, l) && in_viewx(f, l);
}

int
c_in_view(struct f_data_c *f)
{
	assert(f && f->vs);
	return (f->vs->current_link != -1
		&& in_view(f, &f->f_data->links[f->vs->current_link]));
}

int
next_in_view(struct f_data_c *f, int p, int d,
	     int (*fn)(struct f_data_c *, struct link *),
	     void (*cntr)(struct f_data_c *, struct link *))
{
	int p1, p2 = 0;
	int y, yl;

	assert(f && f->f_data && f->vs && fn);

	p1 = f->f_data->nlinks - 1;
	yl = f->vs->view_pos + f->yw;

	if (yl > f->f_data->y) yl = f->f_data->y;
	for (y = f->vs->view_pos < 0 ? 0 : f->vs->view_pos; y < yl; y++) {
		if (f->f_data->lines1[y] && f->f_data->lines1[y] - f->f_data->links < p1)
			p1 = f->f_data->lines1[y] - f->f_data->links;
		if (f->f_data->lines2[y] && f->f_data->lines2[y] - f->f_data->links > p2)
			p2 = f->f_data->lines2[y] - f->f_data->links;
	}
	/*while (p >= 0 && p < f->f_data->nlinks) {*/
	while (p >= p1 && p <= p2) {
		if (fn(f, &f->f_data->links[p])) {
			f->vs->current_link = p;
			if (cntr) cntr(f, &f->f_data->links[p]);
			return 1;
		}
		p += d;
	}
	f->vs->current_link = -1;
	return 0;
}

void
set_pos_x(struct f_data_c *f, struct link *l)
{
	int xm = 0;
	int xl = MAXINT;
	register int i;

	assert(f && l);

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
set_pos_y(struct f_data_c *f, struct link *l)
{
	int ym = 0;
	int yl;
	register int i;

	assert(f && f->f_data && f->vs && l);

	yl = f->f_data->y;
	for (i = 0; i < l->n; i++) {
		if (l->pos[i].y >= ym) ym = l->pos[i].y + 1;
		if (l->pos[i].y < yl) yl = l->pos[i].y;
	}
	f->vs->view_pos = (ym + yl) / 2 - f->f_data->opt.yw / 2;
	if (f->vs->view_pos > f->f_data->y - f->f_data->opt.yw)
		f->vs->view_pos = f->f_data->y - f->f_data->opt.yw;
	if (f->vs->view_pos < 0) f->vs->view_pos = 0;
}

void
find_link(struct f_data_c *f, int p, int s)
{ /* p=1 - top, p=-1 - bottom, s=0 - pgdn, s=1 - down */
	struct link **line;
	struct link *link;
	int y, l;

	assert(f && f->f_data && f->vs);

	if (p == -1) {
		line = f->f_data->lines2;
		if (!line) goto nolink;
		y = f->vs->view_pos + f->yw - 1;
		if (y >= f->f_data->y) y = f->f_data->y - 1;
		if (y < 0) goto nolink;
	} else {
		line = f->f_data->lines1;
		if (!line) goto nolink;
		y = f->vs->view_pos;
		if (y < 0) y = 0;
		if (y >= f->f_data->y) goto nolink;
	}

	link = NULL;
	do {
		if (line[y]
		    && (!link || (p > 0 ? line[y] < link : line[y] > link)))
			link = line[y];
		y += p;
	} while (!(y < 0
		   || y < f->vs->view_pos
		   || y >= f->vs->view_pos + f->f_data->opt.yw
		   || y >= f->f_data->y));

	if (!link) goto nolink;
	l = link - f->f_data->links;

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

static void
page_down(struct session *ses, struct f_data_c *f, int a)
{
	int newpos;

	assert(ses && f && f->vs);

	newpos = f->vs->view_pos + f->f_data->opt.yw;
	if (newpos < f->f_data->y) {
		f->vs->view_pos = newpos;
		find_link(f, 1, a);
	} else {
		find_link(f, -1, a);
	}
}

static void
page_up(struct session *ses, struct f_data_c *f, int a)
{
	assert(ses && f && f->vs);

	f->vs->view_pos -= f->yw;
	find_link(f, -1, a);
	if (f->vs->view_pos < 0) f->vs->view_pos = 0/*, find_link(f, 1, a)*/;
}


static void set_textarea(struct session *, struct f_data_c *, int);
static void jump_to_link_number(struct session *, struct f_data_c *, int);


static void
down(struct session *ses, struct f_data_c *fd, int a)
{
	int current_link;

	assert(ses && fd && fd->vs && fd->f_data);

	current_link = fd->vs->current_link;

	if (get_opt_int("document.browse.links.wraparound")
	    && current_link >= fd->f_data->nlinks - 1) {
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
up(struct session *ses, struct f_data_c *fd, int a)
{
	int current_link;

	assert(ses && fd && fd->vs && fd->f_data);

	current_link = fd->vs->current_link;

	if (get_opt_int("document.browse.links.wraparound")
	    && current_link == 0) {
		jump_to_link_number(ses, fd, fd->f_data->nlinks - 1);
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
scroll(struct session *ses, struct f_data_c *f, int a)
{
	assert(ses && f && f->vs && f->f_data);

	if (f->vs->view_pos + f->f_data->opt.yw >= f->f_data->y && a > 0)
		return;
	f->vs->view_pos += a;
	if (f->vs->view_pos > f->f_data->y - f->f_data->opt.yw && a > 0)
		f->vs->view_pos = f->f_data->y - f->f_data->opt.yw;
	if (f->vs->view_pos < 0) f->vs->view_pos = 0;
	if (c_in_view(f)) return;
	find_link(f, a < 0 ? -1 : 1, 0);
}

static void
hscroll(struct session *ses, struct f_data_c *f, int a)
{
	assert(ses && f && f->vs && f->f_data);

	f->vs->view_posx += a;
	if (f->vs->view_posx >= f->f_data->x)
		f->vs->view_posx = f->f_data->x - 1;
	if (f->vs->view_posx < 0) f->vs->view_posx = 0;
	if (c_in_view(f)) return;
	find_link(f, 1, 0);
	/* !!! FIXME: check right margin */
}

static void
home(struct session *ses, struct f_data_c *f, int a)
{
	assert(ses && f && f->vs);

	f->vs->view_pos = f->vs->view_posx = 0;
	find_link(f, 1, 0);
}

static void
x_end(struct session *ses, struct f_data_c *f, int a)
{
	assert(ses && f && f->vs && f->f_data);

	f->vs->view_posx = 0;
	if (f->vs->view_pos < f->f_data->y - f->f_data->opt.yw)
		f->vs->view_pos = f->f_data->y - f->f_data->opt.yw;
	if (f->vs->view_pos < 0) f->vs->view_pos = 0;
	find_link(f, -1, 0);
}

static int
has_form_submit(struct f_data *f, struct form_control *frm)
{
	struct form_control *i;
	int q = 0;

	assert(f && frm);

	foreach (i, f->forms) if (i->form_num == frm->form_num) {
		if ((i->type == FC_SUBMIT || i->type == FC_IMAGE)) return 1;
		q = 1;
	}
	assertm(q, "form is not on list");
	return 0;
}

static inline void
decrement_fc_refcount(struct f_data *f)
{
	assert(f);

	if (!--f->refcount) format_cache_entries++;
	assertm(f->refcount >= 0, "reference count underflow");
}

struct submitted_value {
	LIST_HEAD(struct submitted_value);

	unsigned char *name;
	unsigned char *value;

	struct form_control *frm;

	void *file_content;

	int fc_len;
	int type;
	int position;
};

static inline void
free_succesful_controls(struct list_head *submit)
{
	struct submitted_value *v;

	assert(submit);

	foreach (v, *submit) {
		if (v->name) mem_free(v->name);
		if (v->value) mem_free(v->value);
		if (v->file_content) mem_free(v->file_content);
	}
	free_list(*submit);
}

static unsigned char *
encode_textarea(struct submitted_value *sv)
{
	unsigned char *text = sv->value;
	unsigned char *newtext;
	int len = 0;
	void *blabla;

	assert(text);

	/* We need to reformat text now if it has to be wrapped
	 * hard, just before encoding it. */
	blabla = format_text(text, sv->frm->cols,
			     sv->frm->wrap);
	if (blabla) mem_free(blabla);

	newtext = init_str();
	if (!newtext) return NULL;

	for (; *text; text++) {
		if (*text != '\n') add_chr_to_str(&newtext, &len, *text);
		else add_to_str(&newtext, &len, "\r\n");
	}

	return newtext;
}

static void
get_succesful_controls(struct f_data_c *f, struct form_control *fc,
		       struct list_head *subm)
{
	struct form_control *frm;
	int ch;

	assert(f && f->f_data && fc && subm);

	init_list(*subm);
	foreach (frm, f->f_data->forms) {
		if (frm->form_num == fc->form_num
		    && ((frm->type != FC_SUBMIT &&
			 frm->type != FC_IMAGE &&
			 frm->type != FC_RESET) || frm == fc)
		    && frm->name && frm->name[0]) {
			struct submitted_value *sub;
			int fi = 0;
			struct form_state *fs = find_form_state(f, frm);

			if (!fs) continue;
			if ((frm->type == FC_CHECKBOX
			     || frm->type == FC_RADIO)
			    && !fs->state)
				continue;
			if (frm->type == FC_SELECT && !frm->nvalues)
				continue;
fi_rep:
			sub = mem_calloc(1, sizeof(struct submitted_value));
			if (!sub) continue;

			sub->type = frm->type;
			sub->name = stracpy(frm->name);

			switch (frm->type) {
				case FC_TEXT:
				case FC_PASSWORD:
				case FC_FILE:
				case FC_TEXTAREA:
					sub->value = stracpy(fs->value);
					break;
				case FC_CHECKBOX:
				case FC_RADIO:
				case FC_SUBMIT:
				case FC_HIDDEN:
					sub->value = stracpy(frm->default_value);
					break;
				case FC_SELECT:
					fixup_select_state(frm, fs);
					sub->value = stracpy(fs->value);
					break;
				case FC_IMAGE:
					add_to_strn(&sub->name, fi ? ".x" : ".y");
					sub->value = stracpy("0");
					break;
				default:
					internal("bad form control type");
					mem_free(sub);
					continue;
			}

			sub->frm = frm;
			sub->position = frm->form_num + frm->ctrl_num;

			add_to_list(*subm, sub);

			if (frm->type == FC_IMAGE && !fi) {
				fi = 1;
				goto fi_rep;
			}
		}
	}

	do {
		struct submitted_value *sub, *nx;

		ch = 0;
		foreach (sub, *subm) if (sub->next != (void *)subm)
			if (sub->next->position < sub->position) {
				nx = sub->next;
				del_from_list(sub);
				add_at_pos(nx, sub);
				sub = nx;
				ch = 1;
			}
		foreachback (sub, *subm) if (sub->next != (void *)subm)
			if (sub->next->position < sub->position) {
				nx = sub->next;
				del_from_list(sub);
				add_at_pos(nx, sub);
				sub = nx;
				ch = 1;
			}
	} while (ch);

}

static inline unsigned char *
strip_file_name(unsigned char *f)
{
	unsigned char *n, *l;

	assert(f);

	l = f - 1;
	for (n = f; *n; n++) if (dir_sep(*n)) l = n;
	return l + 1;
}

static void
encode_controls(struct list_head *l, unsigned char **data, int *len,
		int cp_from, int cp_to)
{
	struct submitted_value *sv;
	struct conv_table *convert_table = NULL;
	int lst = 0;

	assert(l && data && len);

	*data = init_str();
	if (!*data) return;
	*len = 0;

	foreach (sv, *l) {
		unsigned char *p2 = NULL;
		struct document_options o;

		memset(&o, 0, sizeof(o));
		o.plain = 1;
		d_opt = &o;

		if (lst) add_chr_to_str(data, len, '&'); else lst = 1;
		encode_url_string(sv->name, data, len);
		add_chr_to_str(data, len, '=');

		/* Convert back to original encoding (see html_form_control()
		 * for the original recoding). */
		if (sv->type == FC_TEXTAREA) {
			unsigned char *p;

			p = encode_textarea(sv);
			if (p) {
				if (!convert_table)
					convert_table = get_translation_table(cp_from, cp_to);

				p2 = convert_string(convert_table, p,
						    strlen(p));
				mem_free(p);
			}
		} else if (sv->type == FC_TEXT ||
			   sv->type == FC_PASSWORD) {
			if (!convert_table)
				convert_table = get_translation_table(cp_from, cp_to);

			p2 = convert_string(convert_table, sv->value,
					    strlen(sv->value));
		} else {
			p2 = stracpy(sv->value);
		}

		if (p2) {
			encode_url_string(p2, data, len);
			mem_free(p2);
		}
	}
}



#define BL	32

/* FIXME: shouldn't we encode data at send time (in http.c) ? --Zas */
static void
encode_multipart(struct session *ses, struct list_head *l,
		 unsigned char **data, int *len,
		 unsigned char *bound, int cp_from, int cp_to)
{
	struct conv_table *convert_table = NULL;
	struct submitted_value *sv;
	int *nbp, *bound_ptrs = NULL;
	int nbound_ptrs = 0;
	int flg = 0;
	register int i;

	assert(ses && l && data && len && bound);

	*data = init_str();
	if (!*data) return;

	memset(bound, 'x', BL);
	*len = 0;

	foreach (sv, *l) {

bnd:
		add_to_str(data, len, "--");
		if (!(nbound_ptrs & (ALLOC_GR-1))) {
			nbp = mem_realloc(bound_ptrs, (nbound_ptrs + ALLOC_GR) * sizeof(int));
			if (!nbp) goto xx;
			bound_ptrs = nbp;
		}
		bound_ptrs[nbound_ptrs++] = *len;

xx:
		add_bytes_to_str(data, len, bound, BL);
		if (flg) break;
		add_to_str(data, len, "\r\nContent-Disposition: form-data; name=\"");
		add_to_str(data, len, sv->name);
		if (sv->type == FC_FILE) {
#define F_BUFLEN 1024
			int fh, rd;
			unsigned char buffer[F_BUFLEN];

			add_to_str(data, len, "\"; filename=\"");
			add_to_str(data, len, strip_file_name(sv->value));
			/* It sends bad data if the file name contains ", but
			   Netscape does the same */
			/* FIXME: is this a reason ? --Zas */
			add_to_str(data, len, "\"\r\n\r\n");

			if (*sv->value) {
				if (get_opt_int_tree(&cmdline_options, "anonymous"))
					goto encode_error;

				/* FIXME: DO NOT COPY FILE IN MEMORY !! --Zas */
				fh = open(sv->value, O_RDONLY);
				if (fh == -1) goto encode_error;
				set_bin(fh);
				do {
					rd = read(fh, buffer, F_BUFLEN);
					if (rd == -1) goto encode_error;
					if (rd) add_bytes_to_str(data, len, buffer, rd);
				} while (rd);
				close(fh);
			}
#undef F_BUFLEN
		} else {
			struct document_options o;

			add_to_str(data, len, "\"\r\n\r\n");

			memset(&o, 0, sizeof(o));
			o.plain = 1;
			d_opt = &o;

			/* Convert back to original encoding (see
			 * html_form_control() for the original recoding). */
			if (sv->type == FC_TEXT || sv->type == FC_PASSWORD ||
			    sv->type == FC_TEXTAREA) {
				unsigned char *p;

				if (!convert_table)
				       	convert_table = get_translation_table(cp_from,
									      cp_to);

				p = convert_string(convert_table, sv->value,
						   strlen(sv->value));
				if (p) {
					add_to_str(data, len, p);
					mem_free(p);
				}
			} else {
				add_to_str(data, len, sv->value);
			}
		}

		add_to_str(data, len, "\r\n");
	}

	if (!flg) {
		flg = 1;
		goto bnd;
	}

	add_to_str(data, len, "--\r\n");
	memset(bound, '0', BL);

again:
	for (i = 0; i <= *len - BL; i++) {
		int j;

		for (j = 0; j < BL; j++) if ((*data)[i + j] != bound[j]) goto nb;
		for (j = BL - 1; j >= 0; j--)
			if (bound[j]++ >= '9') bound[j] = '0';
			else goto again;
		internal("Could not assing boundary");

nb:;
	}

	for (i = 0; i < nbound_ptrs; i++)
		memcpy(*data + bound_ptrs[i], bound, BL);

	mem_free(bound_ptrs);
	return;

encode_error:
	mem_free(bound_ptrs);
	mem_free(*data), *data = NULL;

	{
	unsigned char *m1, *m2;

	/* XXX: This error message should move elsewhere. --Zas */
	m1 = stracpy(sv->value);
	if (!m1) return;
	m2 = stracpy((unsigned char *) strerror(errno));
	msg_box(ses->tab->term, getml(m1, m2, NULL), MSGBOX_FREE_TEXT,
		N_("Error while posting form"), AL_CENTER,
		msg_text(ses->tab->term, N_("Could not get file %s: %s"),
			 m1, m2),
		ses, 1,
		N_("Cancel"), NULL, B_ENTER | B_ESC);
	}
}

static void
reset_form(struct f_data_c *f, int form_num)
{
	struct form_control *frm;

	assert(f && f->f_data);

	foreach (frm, f->f_data->forms) if (frm->form_num == form_num) {
		struct form_state *fs = find_form_state(f, frm);

		if (fs) init_ctrl(frm, fs);
	}
}

unsigned char *
get_form_url(struct session *ses, struct f_data_c *f,
	     struct form_control *frm)
{
	struct list_head submit;
	unsigned char *data;
	unsigned char *go;
	unsigned char bound[BL];
	int cp_from, cp_to;
	int len;

	assert(ses && ses->tab && ses->tab->term);
	assert(f && f->f_data && frm);

	go = init_str();
	if (!go) return NULL;

	if (frm->type == FC_RESET) {
		reset_form(f, frm->form_num);
		return NULL;
	}
	if (!frm->action) return NULL;

	get_succesful_controls(f, frm, &submit);

	cp_from = get_opt_int_tree(ses->tab->term->spec, "charset");
	cp_to = f->f_data->cp;
	if (frm->method == FM_GET || frm->method == FM_POST)
		encode_controls(&submit, &data, &len, cp_from, cp_to);
	else
		encode_multipart(ses, &submit, &data, &len, bound, cp_from, cp_to);

	if (!data) {
		free_succesful_controls(&submit);
		return NULL;
	}

	if (frm->method == FM_GET) {
		int l = 0;
		unsigned char *pos = strchr(frm->action, '#');

		if (pos) {
			add_bytes_to_str(&go, &l, frm->action, pos - frm->action);
		} else {
			add_to_str(&go, &l, frm->action);
		}

		if (strchr(go, '?'))
			add_chr_to_str(&go, &l, '&');
		else
			add_chr_to_str(&go, &l, '?');

		add_to_str(&go, &l, data);

		if (pos) add_to_str(&go, &l, pos);
	} else {
		int l = 0;
		int i;

		add_to_str(&go, &l, frm->action);
		add_chr_to_str(&go, &l, POST_CHAR);
		if (frm->method == FM_POST) {
			add_to_str(&go, &l, "application/x-www-form-urlencoded\n");
		} else {
			add_to_str(&go, &l, "multipart/form-data; boundary=");
			add_bytes_to_str(&go, &l, bound, BL);
			add_chr_to_str(&go, &l, '\n');
		}
		for (i = 0; i < len; i++) {
			unsigned char p[3];

			ulonghexcat(p, NULL, (int) data[i], 2, '0', 0);
			add_to_str(&go, &l, p);
		}
	}

	mem_free(data);
	free_succesful_controls(&submit);
	return go;
}

#undef BL

static unsigned char *
get_link_url(struct session *ses, struct f_data_c *f,
	     struct link *l)
{
	assert(ses && f && l);

	if (l->type == L_LINK) {
		if (!l->where) return stracpy(l->where_img);
		return stracpy(l->where);
	}
	if (l->type != L_BUTTON && l->type != L_FIELD) return NULL;
	return get_form_url(ses, f, l->form);
}

void
set_frame(struct session *ses, struct f_data_c *f, int a)
{
	assert(ses && ses->screen && f && f->vs);
	if (f == ses->screen) return;
	goto_url(ses, f->vs->url);
}

/* This is common backend for submit_form_do() and enter(). */
int
goto_link(unsigned char *url, unsigned char *target, struct session *ses,
	  int do_reload)
{
	assert(url && ses);

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


/* This is common backend for submit_form() and submit_form_reload(). */
static int
submit_form_do(struct terminal *term, void *xxx, struct session *ses,
	       int do_reload)
{
	struct f_data_c *fd;
	struct link *link;

	assert(term && ses);
	fd = current_frame(ses);

	assert(fd && fd->vs && fd->f_data);
	if (fd->vs->current_link == -1) return 1;
	link = &fd->f_data->links[fd->vs->current_link];

	return goto_link(get_form_url(ses, fd, link->form), link->target, ses, do_reload);
}

static int
submit_form(struct terminal *term, void *xxx, struct session *ses)
{
	assert(term && ses);
	return submit_form_do(term, xxx, ses, 0);
}

static int
submit_form_reload(struct terminal *term, void *xxx, struct session *ses)
{
	assert(term && ses);
	return submit_form_do(term, xxx, ses, 1);
}

static int
enter(struct session *ses, struct f_data_c *fd, int a)
{
	struct link *link;

	assert(ses && fd && fd->vs && fd->f_data);

	if (fd->vs->current_link == -1) return 1;
	link = &fd->f_data->links[fd->vs->current_link];

	if (link->type == L_LINK || link->type == L_BUTTON
	    || ((has_form_submit(fd->f_data, link->form)
		 || get_opt_int("document.browse.forms.auto_submit"))
		&& (link->type == L_FIELD || link->type == L_AREA))) {

		return goto_link(get_link_url(ses, fd, link), link->target, ses, a);

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

			foreach (fc, fd->f_data->forms) {
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

		fd->f_data->refcount++;
		add_empty_window(ses->tab->term,
				 (void (*)(void *)) decrement_fc_refcount,
				 fd->f_data);
		do_select_submenu(ses->tab->term, link->form->menu, ses);

	} else {
		internal("bad link type %d", link->type);
	}

	return 1;
}

void
toggle(struct session *ses, struct f_data_c *f, int a)
{
	assert(ses && f && f->vs);

	f->vs->plain = !f->vs->plain;
	html_interpret(ses);
	draw_formatted(ses);
}

void
selected_item(struct terminal *term, void *pitem, struct session *ses)
{
	int item = (int) pitem;
	struct f_data_c *f;
	struct link *l;
	struct form_state *fs;

	assert(term && ses);
	f = current_frame(ses);

	assert(f && f->vs && f->f_data);
	if (f->vs->current_link == -1) return;
	l = &f->f_data->links[f->vs->current_link];
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
	if (!has_form_submit(f->f_data, l->form)) {
		goto_form(ses, f, l->form, l->target);
	}
#endif
}

int
get_current_state(struct session *ses)
{
	struct f_data_c *f;
	struct link *l;
	struct form_state *fs;

	assert(ses);
	f = current_frame(ses);

	assert(f && f->vs && f->f_data);
	if (f->vs->current_link == -1) return -1;
	l = &f->f_data->links[f->vs->current_link];
	if (l->type != L_SELECT) return -1;
	fs = find_form_state(f, l->form);
	if (fs) return fs->state;
	return -1;
}


/*
 * We use some evil hacking in order to make external textarea editor working.
 * We need to have some way how to be notified that the editor finished and we
 * should reload content of the textarea.  So we use global variable
 * textarea_editor as a flag whether we have one running, and if we have, we
 * just call textarea_edit(1, ...).  Then we recover our state from static
 * variables, reload content of textarea back from file and clean up.
 *
 * Unfortunately, we can't support calling of editor from non-master links
 * session, as it would be extremely ugly to hack (you would have to transfer
 * the content of it back to master somehow, add special flags for not deleting
 * of 'delete' etc) and I'm not going to do that now. Inter-links communication
 * *NEEDS* rewrite, as it looks just like quick messy hack now. --pasky
 */

int textarea_editor = 0;

void
textarea_edit(int op, struct terminal *term_, struct form_control *form_,
	      struct form_state *fs_, struct f_data_c *f_, struct link *l_)
{
	static int form_maxlength;
	static struct form_state *fs;
	static struct terminal *term;
	static struct f_data_c *f;
	static struct link *l;
	static char *fn = NULL;

	if (op == 0 && !term_->master) {
		if (fn) mem_free(fn);
		fn = NULL; fs = NULL;

		msg_box(term_, NULL, 0,
			N_("Error"), AL_CENTER,
			N_("You can do this only on the master terminal"),
			NULL, 1,
			N_("Cancel"), NULL, B_ENTER | B_ESC);
		return;
	}

	if (form_) form_maxlength = form_->maxlength;
	if (fs_) fs = fs_;
	if (f_) f = f_;
	if (l_) l = l_;
	if (term_) term = term_;

	if (op == 0 && !textarea_editor && term->master) {
		FILE *taf;
		char *ed = getenv("EDITOR");
		char *ex;
		int h;

		fn = stracpy("linksarea-XXXXXX");
		if (!fn) {
			fs = NULL;
			return;
		}

		h = mkstemp(fn);
		if (h < 0) {
			mem_free(fn); fn = NULL; fs = NULL;
			return;
		}

		taf = fdopen(h, "w");
		if (!taf) {
			mem_free(fn); fn = NULL; fs = NULL;
			return;
		}

		fwrite(fs->value, strlen(fs->value), 1, taf);
		fclose(taf);

		if (!ed || !*ed) ed = "vi";

		ex = mem_alloc(strlen(ed) + strlen(fn) + 2);
		if (!ex) {
			unlink(fn);
			mem_free(fn); fn = NULL; fs = NULL;
			return;
		}

		sprintf(ex, "%s %s", ed, fn);
		exec_on_terminal(term, ex, "", 1);

		mem_free(ex);

		textarea_editor = 1;

	} else if (op == 1 && fs) {
		FILE *taf = fopen(fn, "r+");

		if (taf) {
			int flen = -1;

			if (!fseek(taf, 0, SEEK_END)) {
				flen = ftell(taf);
				if (flen != -1
				    && fseek(taf, 0, SEEK_SET))
					flen = -1;
			}

			if (flen >= 0 && flen <= form_maxlength) {
				int bread;

				mem_free(fs->value);
				fs->value = mem_alloc(flen + 1);
				if (!fs->value) goto close;

				bread = fread(fs->value, 1, flen, taf);
				fs->value[bread] = 0;
				fs->state = bread;

				if (f && l)
					draw_form_entry(term, f, l);
			}

close:
			fclose(taf);
			unlink(fn);
		}

		mem_free(fn); fn = NULL; fs = NULL;
		textarea_editor = 0;
	}
}


/* TODO: Unify the textarea field_op handlers to one trampoline function. */

static int
textarea_op_home(struct form_state *fs, struct form_control *frm, int rep)
{
	struct line_info *ln;
	int y;

	ln = format_text(fs->value, frm->cols, !!frm->wrap);
	if (!ln) return 0;

	for (y = 0; ln[y].st; y++) {
		if (fs->value + fs->state >= ln[y].st &&
		    fs->value + fs->state < ln[y].en + (ln[y+1].st != ln[y].en)) {
			fs->state = ln[y].st - fs->value;
			goto x;
		}
	}
	fs->state = 0;

x:
	mem_free(ln);
	return 0;
}

static int
textarea_op_up(struct form_state *fs, struct form_control *frm, int rep)
{
	struct line_info *ln;
	int y;

	ln = format_text(fs->value, frm->cols, !!frm->wrap);
	if (!ln) return 0;

rep1:
	for (y = 0; ln[y].st; y++) {
		if (fs->value + fs->state >= ln[y].st &&
		    fs->value + fs->state < ln[y].en + (ln[y+1].st != ln[y].en)) {
			if (!y) {
				mem_free(ln);
				return 1;
			}
			fs->state -= ln[y].st - ln[y-1].st;
			if (fs->value + fs->state > ln[y-1].en)
				fs->state = ln[y-1].en - fs->value;
			goto xx;
		}
	}
	mem_free(ln);
	return 1;

xx:
	if (rep) goto rep1;
	mem_free(ln);
	return 0;
}

static int
textarea_op_down(struct form_state *fs, struct form_control *frm, int rep)
{
	struct line_info *ln;
	int y;

	ln = format_text(fs->value, frm->cols, !!frm->wrap);
	if (!ln) return 0;

rep2:
	for (y = 0; ln[y].st; y++) {
		if (fs->value + fs->state >= ln[y].st &&
		    fs->value + fs->state < ln[y].en + (ln[y+1].st != ln[y].en)) {
			if (!ln[y+1].st) {
				mem_free(ln);
				return 1;
			}
			fs->state += ln[y+1].st - ln[y].st;
			if (fs->value + fs->state > ln[y+1].en)
				fs->state = ln[y+1].en - fs->value;
			goto yy;
		}
	}
	mem_free(ln);
	return 1;
yy:
	if (rep) goto rep2;
	mem_free(ln);
	return 0;
}

static int
textarea_op_end(struct form_state *fs, struct form_control *frm, int rep)
{
	struct line_info *ln;
	int y;

	ln = format_text(fs->value, frm->cols, !!frm->wrap);
	if (!ln) return 0;

	for (y = 0; ln[y].st; y++) {
		if (fs->value + fs->state >= ln[y].st &&
		    fs->value + fs->state < ln[y].en + (ln[y+1].st != ln[y].en)) {
			fs->state = ln[y].en - fs->value;

			/* Don't jump to next line when wrapping. */
			if (fs->state && fs->state < strlen(fs->value)
			    && ln[y+1].st == ln[y].en)
				fs->state--;

			goto yyyy;
		}
	}
	fs->state = strlen(fs->value);
yyyy:
	mem_free(ln);
	return 0;
}

static int
textarea_op_enter(struct form_state *fs, struct form_control *frm, int rep)
{
	if (!frm->ro && strlen(fs->value) < frm->maxlength) {
		unsigned char *v = mem_realloc(fs->value, strlen(fs->value) + 2);

		if (v) {
			fs->value = v;
			memmove(v + fs->state + 1, v + fs->state, strlen(v + fs->state) + 1);
			v[fs->state++] = '\n';
		}
	}

	return 0;
}


static int
field_op(struct session *ses, struct f_data_c *f, struct link *l,
	 struct event *ev, int rep)
{
	struct form_control *frm;
	struct form_state *fs;
	int x = 1;

	assert(ses && f && l && ev);
	frm = l->form;
	assertm(frm, "link has no form control");

	if (l->form->ro == 2) return 0;
	fs = find_form_state(f, frm);
	if (!fs || !fs->value) return 0;

	if (ev->ev == EV_KBD) {
		switch (kbd_action(KM_EDIT, ev, NULL)) {
			case ACT_LEFT:
				fs->state = fs->state ? fs->state - 1 : 0;
				break;
			case ACT_RIGHT:
				{
					int fsv_len = strlen(fs->value);

					fs->state = fs->state < fsv_len
						    ? fs->state + 1
						      : fsv_len;
				}
				break;
			case ACT_HOME:
				if (frm->type == FC_TEXTAREA) {
					if (textarea_op_home(fs, frm, rep))
						goto b;
				} else fs->state = 0;
				break;
			case ACT_UP:
				if (frm->type == FC_TEXTAREA) {
					if (textarea_op_up(fs, frm, rep))
						goto b;
				} else x = 0;
				break;
			case ACT_DOWN:
				if (frm->type == FC_TEXTAREA) {
					if (textarea_op_down(fs, frm, rep))
						goto b;
				} else x = 0;
				break;
			case ACT_END:
				if (frm->type == FC_TEXTAREA) {
					if (textarea_op_end(fs, frm, rep))
						goto b;
				} else fs->state = strlen(fs->value);
				break;
			case ACT_EDIT:
				if (frm->type == FC_TEXTAREA && !frm->ro)
				  	textarea_edit(0, ses->tab->term, frm, fs, f, l);
				break;
			case ACT_COPY_CLIPBOARD:
				set_clipboard_text(fs->value);
				break;
			case ACT_CUT_CLIPBOARD:
				set_clipboard_text(fs->value);
				if (!frm->ro) fs->value[0] = 0;
				fs->state = 0;
				break;
			case ACT_PASTE_CLIPBOARD: {
				char *clipboard = get_clipboard_text();

				if (!clipboard) break;
				if (!frm->ro) {
					int cb_len = strlen(clipboard);

					if (cb_len <= frm->maxlength) {
						unsigned char *v = mem_realloc(fs->value, cb_len + 1);

						if (v) {
							fs->value = v;
							memmove(v, clipboard, cb_len + 1);
							fs->state = strlen(fs->value);
						}
					}
				}
				mem_free(clipboard);
				break;
			}
			case ACT_ENTER:
				if (frm->type == FC_TEXTAREA) {
					if (textarea_op_enter(fs, frm, rep))
						goto b;
				} else x = 0;
				break;
			case ACT_BACKSPACE:
				if (!frm->ro && fs->state)
					memmove(fs->value + fs->state - 1, fs->value + fs->state,
						strlen(fs->value + fs->state) + 1),
					fs->state--;
				break;
			case ACT_DELETE:
				if (!frm->ro && fs->state < strlen(fs->value))
					memmove(fs->value + fs->state, fs->value + fs->state + 1,
						strlen(fs->value + fs->state));
				break;
			case ACT_KILL_TO_BOL:
				if (!frm->ro)
					memmove(fs->value, fs->value + fs->state,
						strlen(fs->value + fs->state) + 1);
				fs->state = 0;
				break;
		    	case ACT_KILL_TO_EOL:
				fs->value[fs->state] = 0;
				break;
			default:
				if (!ev->y && (ev->x >= 32 && ev->x < 256)) {
					if (!frm->ro && strlen(fs->value) < frm->maxlength) {
						unsigned char *v = mem_realloc(fs->value, strlen(fs->value) + 2);

						if (v) {
							fs->value = v;
							memmove(v + fs->state + 1, v + fs->state, strlen(v + fs->state) + 1);
							v[fs->state++] = ev->x;
						}
					}
				} else {

b:
					x = 0;
				}
		}
	} else x = 0;

	if (x) {
		draw_form_entry(ses->tab->term, f, l);
		redraw_from_window(ses->tab);
	}
	return x;
}

static void
set_textarea(struct session *ses, struct f_data_c *f, int kbd)
{
	assert(ses && f && f->vs && f->f_data);

	if (f->vs->current_link != -1
	    && f->f_data->links[f->vs->current_link].type == L_AREA) {
		struct event ev = { EV_KBD, 0, 0, 0 };

		ev.x = kbd;
		field_op(ses, f, &f->f_data->links[f->vs->current_link], &ev, 1);
	}
}


static inline void
rep_ev(struct session *ses, struct f_data_c *fd,
       void (*f)(struct session *, struct f_data_c *, int),
       int a)
{
	register int i;

	assert(ses && fd && f);

	i = ses->kbdprefix.rep ? ses->kbdprefix.rep_num : 1;
	while (i--) f(ses, fd, a);
}

static struct link *
choose_mouse_link(struct f_data_c *f, struct event *ev)
{
	struct link *l1, *l2, *l;
	register int i;

	assert(f && f->vs && f->f_data && ev);

	l1 = f->f_data->links + f->f_data->nlinks;
	l2 = f->f_data->links;

	if (!f->f_data->nlinks
	    || ev->x < 0 || ev->y < 0 || ev->x >= f->xw || ev->y >= f->yw)
		return NULL;

	for (i = f->vs->view_pos;
	     i < f->f_data->y && i < f->vs->view_pos + f->yw;
	     i++) {
		if (f->f_data->lines1[i] && f->f_data->lines1[i] < l1)
			l1 = f->f_data->lines1[i];
		if (f->f_data->lines2[i] && f->f_data->lines2[i] > l2)
			l2 = f->f_data->lines2[i];
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
static void
jump_to_link_number(struct session *ses, struct f_data_c *fd, int n)
{
	assert(ses && fd && fd->vs);

	if (n < 0 || n > fd->f_data->nlinks) return;
	fd->vs->current_link = n;
	check_vs(fd);
}

/* This is common backend for goto_link_number() and try_document_key(). */
static void
goto_link_number_do(struct session *ses, struct f_data_c *fd, int n)
{
	struct link *link;

	assert(ses && fd && fd->f_data);
	if (n < 0 || n > fd->f_data->nlinks) return;
	jump_to_link_number(ses, fd, n);

	link = &fd->f_data->links[n];
	if (link->type != L_AREA
	    && link->type != L_FIELD
	    && get_opt_int("document.browse.accesskey.auto_follow"))
		enter(ses, fd, 0);
}

static void
goto_link_number(struct session *ses, unsigned char *num)
{
	struct f_data_c *fd;

	assert(ses && num);
	fd = current_frame(ses);
	assert(fd);
	goto_link_number_do(ses, fd, atoi(num) - 1);
}

/* See if this document is interested in the key user pressed. */
static int
try_document_key(struct session *ses, struct f_data_c *fd,
		 struct event *ev)
{
	long x;
	int passed = -1;
	int i; /* GOD I HATE C! --FF */ /* YEAH, BRAINFUCK RULEZ! --pasky */

	assert(ses && fd && fd->f_data && fd->vs && ev);

	x = (ev->x < 0x100) ? upcase(ev->x) : ev->x;
	if (x >= 'A' && x <= 'Z' && ev->y != KBD_ALT) {
		/* We accept those only in alt-combo. */
		return 0;
	}

	/* Run through all the links and see if one of them is bound to the
	 * key we test.. */

	for (i = 0; i < fd->f_data->nlinks; i++) {
		struct link *link = &fd->f_data->links[i];

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

		if (i == fd->f_data->nlinks - 1 && passed >= 0) {
			/* Return to the start. */
			i = passed - 1;
		}
	}

	return 0;
}

void frm_download(struct session *, struct f_data_c *, int resume);
void send_image(struct terminal *term, void *xxx, struct session *ses);
void send_download_image(struct terminal *term, void *xxx, struct session *ses);

static int
frame_ev(struct session *ses, struct f_data_c *fd, struct event *ev)
{
	int x = 1;

	assert(ses && fd && fd->f_data && fd->vs && ev);

	if (fd->vs->current_link >= 0
	    && (fd->f_data->links[fd->vs->current_link].type == L_FIELD ||
		fd->f_data->links[fd->vs->current_link].type == L_AREA)
	    && field_op(ses, fd, &fd->f_data->links[fd->vs->current_link], ev, 0))
		return 1;

	if (ev->ev == EV_KBD) {
		if (ev->x >= '0' + !ses->kbdprefix.rep && ev->x <= '9'
		    && (ev->y
			|| !fd->f_data->opt.num_links_key
			|| (fd->f_data->opt.num_links_key == 1
			    && !fd->f_data->opt.num_links_display))) {
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
				    > fd->f_data->nlinks) {
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
			case ACT_DOWNLOAD: if (!get_opt_int_tree(&cmdline_options, "anonymous")) frm_download(ses, fd, 0); break;
			case ACT_RESUME_DOWNLOAD: if (!get_opt_int_tree(&cmdline_options, "anonymous")) frm_download(ses, fd, 1); break;
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

					struct f_data *f_data = fd->f_data;
					int nl, lnl;
					unsigned char d[2];

					d[0] = ev->x;
					d[1] = 0;
					nl = f_data->nlinks, lnl = 1;
					while (nl) nl /= 10, lnl++;
					if (lnl > 1)
						input_field(ses->tab->term, NULL, 1,
							    N_("Go to link"), N_("Enter link number"),
							    N_("OK"), N_("Cancel"), ses, NULL,
							    lnl, d, 1, f_data->nlinks, check_number,
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
					foreachback (node, fd->f_data->nodes) {
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
			fd->vs->current_link = link - fd->f_data->links;

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

struct f_data_c *
current_frame(struct session *ses)
{
	struct f_data_c *fd = NULL;
	int i;

	assert(ses);

	if (!have_location(ses)) return NULL;
	i = cur_loc(ses)->vs.current_link;
	foreach (fd, ses->scrn_frames) {
		if (fd->f_data && fd->f_data->frame) continue;
		if (!i--) return fd;
	}
	fd = cur_loc(ses)->vs.f;
	/* The fd test probably only hides bugs in history handling. --pasky */
	if (/*fd &&*/ fd->f_data && fd->f_data->frame) return NULL;
	return fd;
}

static int
send_to_frame(struct session *ses, struct event *ev)
{
	struct f_data_c *fd;
	int r;

	assert(ses && ses->tab && ses->tab->term && ev);
	fd = current_frame(ses);
	assertm(fd, "document not formatted");

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
	     void (*f)(struct session *, struct f_data_c *, int),
	     int a)
{
	struct f_data_c *fd;

	assert(ses && f);
	fd = current_frame(ses);
	assertm(fd, "document not formatted");

	f(ses, fd, a);
}

static void
do_mouse_event(struct session *ses, struct event *ev)
{
	struct event evv;
	struct f_data_c *fdd, *fd; /* !!! FIXME: frames */
	struct document_options *o;

	assert(ses && ev);
	fd = current_frame(ses);
	assert(fd && fd->f_data);

	o = &fd->f_data->opt;
	if (ev->x >= o->xp && ev->x < o->xp + o->xw &&
	    ev->y >= o->yp && ev->y < o->yp + o->yw) goto ok;

r:
	next_frame(ses, 1);
	fdd = current_frame(ses);
	assert(fdd && fdd->f_data);
	o = &fdd->f_data->opt;
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
	struct f_data_c *fd;

	assert(ses && ev);
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
				if (!get_opt_int_tree(&cmdline_options, "anonymous"))
					launch_bm_add_doc_dialog(ses->tab->term, NULL, ses);
#endif
				goto x;
			case ACT_ADD_BOOKMARK_LINK:
#ifdef BOOKMARKS
				if (!get_opt_int_tree(&cmdline_options, "anonymous"))
					launch_bm_add_link_dialog(ses->tab->term, NULL, ses);
#endif
				goto x;
			case ACT_BOOKMARK_MANAGER:
#ifdef BOOKMARKS
				if (!get_opt_int_tree(&cmdline_options, "anonymous"))
					menu_bookmark_manager(ses->tab->term, NULL, ses);
#endif
				goto x;
			case ACT_HISTORY_MANAGER:
#ifdef GLOBHIST
				if (!get_opt_int_tree(&cmdline_options, "anonymous"))
					menu_history_manager(ses->tab->term, NULL, ses);
#endif
				goto x;
			case ACT_OPTIONS_MANAGER:
				if (!get_opt_int_tree(&cmdline_options, "anonymous"))
					menu_options_manager(ses->tab->term, NULL, ses);
				goto x;
			case ACT_KEYBINDING_MANAGER:
				if (!get_opt_int_tree(&cmdline_options, "anonymous"))
					menu_keybinding_manager(ses->tab->term, NULL, ses);
				goto x;
			case ACT_COOKIES_LOAD:
#ifdef COOKIES
				if (!get_opt_int_tree(&cmdline_options, "anonymous")
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

static void
send_enter(struct terminal *term, void *xxx, struct session *ses)
{
	struct event ev = { EV_KBD, KBD_ENTER, 0, 0 };

	assert(ses);
	send_event(ses, &ev);
}

static void
send_enter_reload(struct terminal *term, void *xxx, struct session *ses)
{
	struct event ev = { EV_KBD, KBD_ENTER, KBD_CTRL, 0 };

	assert(ses);
	send_event(ses, &ev);
}

void
frm_download(struct session *ses, struct f_data_c *fd, int resume)
{
	struct link *link;

	assert(ses && fd && fd->vs && fd->f_data);

	if (fd->vs->current_link == -1) return;
	if (ses->dn_url) {
		mem_free(ses->dn_url);
		ses->dn_url = NULL;
	}
	link = &fd->f_data->links[fd->vs->current_link];
	if (link->type != L_LINK && link->type != L_BUTTON) return;

	ses->dn_url = get_link_url(ses, fd, link);
	if (ses->dn_url) {
		if (!strncasecmp(ses->dn_url, "MAP@", 4)) {
			mem_free(ses->dn_url);
			ses->dn_url = NULL;
			return;
		}
		if (ses->ref_url) mem_free(ses->ref_url);
		ses->ref_url = stracpy(fd->f_data->url);
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
	struct f_data_c *fd;

	assert(term && ses);
	fd = current_frame(ses);
	assert(fd && fd->vs && fd->f_data);

	if (fd->vs->current_link == -1) return;
	if (ses->dn_url) {
		mem_free(ses->dn_url);
		ses->dn_url = NULL;
	}

	if (dlt == URL) {
		ses->dn_url = get_link_url(ses, fd, &fd->f_data->links[fd->vs->current_link]);
	} else if (dlt == IMAGE) {
		unsigned char *wi = fd->f_data->links[fd->vs->current_link].where_img;

		if (wi) ses->dn_url = stracpy(wi);
	} else {
		internal("Unknown dl_type");
		ses->dn_url = NULL;
		return;
	}

	if (ses->dn_url) {
		if (ses->ref_url) mem_free(ses->ref_url);
		ses->ref_url = stracpy(fd->f_data->url);
		query_file(ses, ses->dn_url, start_download, NULL, 1);
	}
}


void
send_download_image(struct terminal *term, void *xxx, struct session *ses)
{
	assert(term && ses);
	send_download_do(term, xxx, ses, IMAGE);
}

static void
send_download(struct terminal *term, void *xxx, struct session *ses)
{
	assert(term && ses);
	send_download_do(term, xxx, ses, URL);
}

static int
add_session_ring_to_str(unsigned char **str, int *len)
{
	int ring;

	assert(str && len);

	ring = get_opt_int_tree(&cmdline_options, "session-ring");
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
	struct f_data_c *fd;

	assert(term && open_window && ses);
	fd = current_frame(ses);
	assert(fd && fd->vs && fd->f_data);

	if (fd->vs->current_link == -1) return;
	if (ses->dn_url) mem_free(ses->dn_url);
	ses->dn_url = get_link_url(ses, fd, &fd->f_data->links[fd->vs->current_link]);
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
	struct f_data_c *fd;
	unsigned char *u;

	assert(ses && ses->tab && ses->tab->term && url);
	u = translate_url(url, ses->tab->term->cwd);

	if (!u) {
		struct status stat = { NULL_LIST_HEAD, NULL, NULL,
				       NULL, NULL, NULL,
				       S_BAD_URL, PRI_CANCEL, 0 };

		print_error_dialog(ses, &stat);
		return;
	}

	if (ses->dn_url) mem_free(ses->dn_url);
	ses->dn_url = u;

	if (ses->ref_url) mem_free(ses->ref_url);

	fd = current_frame(ses);
	assert(fd && fd->f_data && fd->f_data->url);

	ses->ref_url = stracpy(fd->f_data->url);
	query_file(ses, ses->dn_url, start_download, NULL, 1);
}

void
send_image(struct terminal *term, void *xxx, struct session *ses)
{
	struct f_data_c *fd;
	unsigned char *u;

	assert(term && ses);
	fd = current_frame(ses);
	assert(fd && fd->f_data && fd->vs);

	if (fd->vs->current_link == -1) return;
	u = fd->f_data->links[fd->vs->current_link].where_img;
	if (!u) return;
	goto_url(ses, u);
}

void
save_as(struct terminal *term, void *xxx, struct session *ses)
{
	struct location *l;

	assert(term && ses);

	if (!have_location(ses)) return;
	l = cur_loc(ses);
	if (ses->dn_url) mem_free(ses->dn_url);
	ses->dn_url = stracpy(l->vs.url);
	if (ses->dn_url) {
		struct f_data_c *fd = current_frame(ses);

		assert(fd && fd->f_data && fd->f_data->url);

		if (ses->ref_url) mem_free(ses->ref_url);
		ses->ref_url = stracpy(fd->f_data->url);
		query_file(ses, ses->dn_url, start_download, NULL, 1);
	}
}

static void
save_formatted_finish(struct terminal *term, int h, void *data, int resume)
{
	struct f_data *f_data = data;

	assert(term && f_data);

	if (h == -1) return;
	if (dump_to_file(f_data, h)) {
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
	struct f_data_c *fd;

	assert(ses && ses->tab && ses->tab->term && file);
	fd = current_frame(ses);
	assert(fd && fd->f_data);

	create_download_file(ses->tab->term, file, NULL, 0, 0,
			     save_formatted_finish, fd->f_data);
}

void
menu_save_formatted(struct terminal *term, void *xxx, struct session *ses)
{
	struct f_data_c *fd;

	assert(term && ses);
	fd = current_frame(ses);
	assert(fd && fd->vs);

	query_file(ses, fd->vs->url, save_formatted, NULL, !((int) xxx));
}

/* Open a contextual menu on a link, form or image element. */
/* TODO: This should be completely configurable. */
void
link_menu(struct terminal *term, void *xxx, struct session *ses)
{
	struct f_data_c *fd;
	struct link *link;
	struct menu_item *mi;
	int l = 0;

	assert(term && ses);

	fd = current_frame(ses);
	mi = new_menu(FREE_LIST);
	if (!mi) return;
	if (!fd) goto end;

	assert(fd->vs && fd->f_data);
	if (fd->vs->current_link < 0) goto end;

	link = &fd->f_data->links[fd->vs->current_link];
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

			if (!get_opt_int_tree(&cmdline_options, "anonymous")) {
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

			if (!get_opt_int_tree(&cmdline_options, "anonymous"))
				add_to_menu(&mi, N_("Submit form and ~download"), "d",
					    MENU_FUNC send_download, NULL, 0, 0);
		}
	}

	if (link->where_img) {
		l = 1;
		add_to_menu(&mi, N_("V~iew image"), "",
			    MENU_FUNC send_image, NULL, 0, 0);
		if (!get_opt_int_tree(&cmdline_options, "anonymous"))
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
print_current_link_title_do(struct f_data_c *fd, struct terminal *term)
{
	struct link *link;

	assert(term && fd && fd->f_data && fd->vs);

	if (fd->f_data->frame || fd->vs->current_link == -1
	    || fd->vs->current_link >= fd->f_data->nlinks)
		return NULL;

	link = &fd->f_data->links[fd->vs->current_link];

	if (link->title)
		return stracpy(link->title);

	return NULL;
}


unsigned char *
print_current_link_do(struct f_data_c *fd, struct terminal *term)
{
	struct link *link;

	assert(term && fd && fd->f_data && fd->vs);

	if (fd->f_data->frame || fd->vs->current_link == -1
	    || fd->vs->current_link >= fd->f_data->nlinks) {
		return NULL;
	}

	link = &fd->f_data->links[fd->vs->current_link];

	if (link->type == L_LINK) {
		if (!link->where && link->where_img) {
			unsigned char *url;
			unsigned char *str = init_str();
			int strl = 0;

			if (!str) return NULL;

			add_to_str(&str, &strl, _("Image", term));
			add_chr_to_str(&str, &strl, ' ');
			url = strip_url_password(link->where_img);
			if (url) {
				add_to_str(&str, &strl, url);
				mem_free(url);
			}
			return str;
		}

		if (strlen(link->where) >= 4
		    && !strncasecmp(link->where, "MAP@", 4)) {
			unsigned char *url;
			unsigned char *str = init_str();
			int strl = 0;

			if (!str) return NULL;

			add_to_str(&str, &strl, _("Usemap", term));
			add_chr_to_str(&str, &strl, ' ');
			url = strip_url_password(link->where + 4);
			if (url) {
				add_to_str(&str, &strl, url);
				mem_free(url);
			}
			return str;
		}

		return strip_url_password(link->where);
	}

	if (!link->form) return NULL;

	if (link->type == L_BUTTON) {
		unsigned char *url;
		unsigned char *str;
		int strl = 0;

		if (link->form->type == FC_RESET)
			return stracpy(_("~Reset form", term));

		if (!link->form->action) return NULL;

		str = init_str();
		if (!str) return NULL;

		if (link->form->method == FM_GET)
			add_to_str(&str, &strl, _("Submit form to", term));
		else
			add_to_str(&str, &strl, _("Post form to", term));
		add_chr_to_str(&str, &strl, ' ');

		url = strip_url_password(link->form->action);
		if (url) {
			add_to_str(&str, &strl, url);
			mem_free(url);
		}
		return str;
	}

	if (link->type == L_CHECKBOX || link->type == L_SELECT
	    || link->type == L_FIELD || link->type == L_AREA) {
		unsigned char * str = init_str();
		int strl = 0;

		if (!str) return NULL;

		if (link->form->type == FC_RADIO)
			add_to_str(&str, &strl, _("Radio button", term));

		else if (link->form->type == FC_CHECKBOX)
			add_to_str(&str, &strl, _("Checkbox", term));

		else if (link->form->type == FC_SELECT)
			add_to_str(&str, &strl, _("Select field", term));

		else if (link->form->type == FC_TEXT)
			add_to_str(&str, &strl, _("Text field", term));

		else if (link->form->type == FC_TEXTAREA)
			add_to_str(&str, &strl, _("Text area", term));

		else if (link->form->type == FC_FILE)
			add_to_str(&str, &strl, _("File upload", term));

		else if (link->form->type == FC_PASSWORD)
			add_to_str(&str, &strl, _("Password field", term));

		else {
			mem_free(str);
			return NULL;
		}

		if (link->form->name && link->form->name[0]) {
			add_to_str(&str, &strl, ", ");
			add_to_str(&str, &strl, _("name", term));
			add_chr_to_str(&str, &strl, ' ');
			add_to_str(&str, &strl, link->form->name);
		}

		if ((link->form->type == FC_CHECKBOX ||
		     link->form->type == FC_RADIO)
		    && link->form->default_value
		    && link->form->default_value[0]) {
			add_to_str(&str, &strl, ", ");
			add_to_str(&str, &strl, _("value", term));
			add_chr_to_str(&str, &strl, ' ');
			add_to_str(&str, &strl, link->form->default_value);
		}

		if (link->type == L_FIELD
		    && !has_form_submit(fd->f_data, link->form)
		    && link->form->action) {
			unsigned char *url;

			add_to_str(&str, &strl, ", ");
			add_to_str(&str, &strl, _("hit ENTER to", term));
			add_chr_to_str(&str, &strl, ' ');
			if (link->form->method == FM_GET)
				add_to_str(&str, &strl, _("submit to", term));
			else
				add_to_str(&str, &strl, _("post to", term));
			add_chr_to_str(&str, &strl, ' ');
			url = strip_url_password(link->form->action);
			if (url) {
				add_to_str(&str, &strl, url);
				mem_free(url);
			}
		}

		return str;
	}

	/* Uh-oh? */
	return NULL;
}

unsigned char *
print_current_link(struct session *ses)
{
	struct f_data_c *fd;

	assert(ses && ses->tab && ses->tab->term);
	fd = current_frame(ses);
	assert(fd);

	return print_current_link_do(fd, ses->tab->term);
}


/* Print page's title and numbering at window top. */
static unsigned char *
print_current_titlex(struct f_data_c *fd, int w)
{
	int ml = 0, pl = 0;
	unsigned char *m;
	unsigned char *p;

	assert(fd);

	p = init_str();
	if (!p) return NULL;

	if (fd->yw < fd->f_data->y) {
		int pp = 1;
		int pe = 1;

		if (fd->yw) {
			pp = (fd->vs->view_pos + fd->yw / 2) / fd->yw + 1;
			pe = (fd->f_data->y + fd->yw - 1) / fd->yw;
			if (pp > pe) pp = pe;
		}

		if (fd->vs->view_pos + fd->yw >= fd->f_data->y)
			pp = pe;
		if (fd->f_data->title)
			add_chr_to_str(&p, &pl, ' ');

		add_chr_to_str(&p, &pl, '(');
		add_num_to_str(&p, &pl, pp);
		add_chr_to_str(&p, &pl, '/');
		add_num_to_str(&p, &pl, pe);
		add_chr_to_str(&p, &pl, ')');
	}

	if (!fd->f_data->title) return p;

	m = init_str();
	if (!m) goto end;

	add_to_str(&m, &ml, fd->f_data->title);

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
	struct f_data_c *fd;

	assert(ses && ses->tab && ses->tab->term);
	fd = current_frame(ses);
	assert(fd);

	return print_current_titlex(fd, ses->tab->term->x);
}
