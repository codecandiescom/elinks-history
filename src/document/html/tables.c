/* HTML tables renderer */
/* $Id: tables.c,v 1.75 2003/09/15 21:11:41 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/html/parser.h"
#include "document/html/renderer.h"
#include "document/html/tables.h"
#include "terminal/draw.h"
#include "util/color.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"

#define table table_dirty_workaround_for_name_clash_with_libraries_on_macos

#define AL_TR		-1

#define VAL_TR		-1
#define VAL_TOP		0
#define VAL_MIDDLE	1
#define VAL_BOTTOM	2

#define W_AUTO		-1
#define W_REL		-2

#define F_VOID		0
#define F_ABOVE		1
#define F_BELOW		2
#define F_HSIDES	3
#define F_LHS		4
#define F_RHS		8
#define F_VSIDES	12
#define F_BOX		15

#define R_NONE		0
#define R_ROWS		1
#define R_COLS		2
#define R_ALL		3
#define R_GROUPS	4

#define INIT_X		2
#define INIT_Y		2

#define CELL(t, x, y) (&(t)->cells[(y) * (t)->rx + (x)])

/* Types and structs */

struct table_cell {
	unsigned char *start;
	unsigned char *end;
	color_t bgcolor;
	int mx, my;
	int align;
	int valign;
	int group;
	int colspan;
	int rowspan;
	int min_width;
	int max_width;
	int x_width;
	int height;
	int link_num;

	unsigned int used:1;
	unsigned int spanned:1;
	unsigned int b:1;
};

struct table_column {
	int group;
	int align;
	int valign;
	int width;
};

struct table {
	struct part *p;
	struct table_cell *cells;
	struct table_column *cols;
	color_t bgcolor;
	int *min_c, *max_c;
	int *w_c;
	int *xcols;
	int *r_heights;
	int x, y;
	int rx, ry;
	int border, cellpd, vcellpd, cellsp;
	int frame, rules, width, wf;
	int rw;
	int min_t, max_t;
	int c, rc;
	int xc;
	int rh;
	int link_num;
};

struct s_e {
	unsigned char *s, *e;
};



/* Global variables */

int table_level;

/* TODO: Use /FRAME._.* / macros ! --pasky */

static unsigned char frame_table[81] = {
	0x00, 0xb3, 0xba,	0xc4, 0xc0, 0xd3,	0xcd, 0xd4, 0xc8,
	0xc4, 0xd9, 0xbd,	0xc4, 0xc1, 0xd0,	0xcd, 0xd4, 0xc8,
	0xcd, 0xbe, 0xbc,	0xcd, 0xbe, 0xbc,	0xcd, 0xcf, 0xca,

	0xb3, 0xb3, 0xba,	0xda, 0xc3, 0xd3,	0xd5, 0xc6, 0xc8,
	0xbf, 0xb4, 0xbd,	0xc2, 0xc5, 0xd0,	0xd5, 0xc6, 0xc8,
	0xb8, 0xb5, 0xbc,	0xb8, 0xb5, 0xbc,	0xd1, 0xd8, 0xca,

	0xba, 0xba, 0xba,	0xd6, 0xd6, 0xc7,	0xc9, 0xc9, 0xcc,
	0xb7, 0xb7, 0xb6,	0xd2, 0xd2, 0xd7,	0xc9, 0xc9, 0xcc,
	0xbb, 0xbb, 0xb9,	0xbb, 0xbb, 0xb9,	0xcb, 0xcb, 0xce,
};

static unsigned char hline_table[3] = { 0x20, 0xc4, 0xcd };
static unsigned char vline_table[3] = { 0x20, 0xb3, 0xba };


static inline void
get_align(unsigned char *attr, int *a)
{
	unsigned char *al = get_attr_val(attr, "align");

	if (al) {
		if (!(strcasecmp(al, "left"))) *a = AL_LEFT;
		else if (!(strcasecmp(al, "right"))) *a = AL_RIGHT;
		else if (!(strcasecmp(al, "center"))) *a = AL_CENTER;
		else if (!(strcasecmp(al, "justify"))) *a = AL_BLOCK;
		else if (!(strcasecmp(al, "char"))) *a = AL_RIGHT; /* NOT IMPLEMENTED */
		mem_free(al);
	}
}

static inline void
get_valign(unsigned char *attr, int *a)
{
	unsigned char *al = get_attr_val(attr, "valign");

	if (al) {
		if (!(strcasecmp(al, "top"))) *a = VAL_TOP;
		else if (!(strcasecmp(al, "middle"))) *a = VAL_MIDDLE;
		else if (!(strcasecmp(al, "bottom"))) *a = VAL_BOTTOM;
		else if (!(strcasecmp(al, "baseline"))) *a = VAL_TOP; /* NOT IMPLEMENTED */
		mem_free(al);
	}
}

static inline void
get_c_width(unsigned char *attr, int *w, int sh)
{
	unsigned char *al = get_attr_val(attr, "width");

	if (al) {
		int len = strlen(al);

		if (len && al[len - 1] == '*') {
			unsigned char *en;
			int n;

			al[len - 1] = '\0';
			errno = 0;
			n = strtoul(al, (char **)&en, 10);
			if (!errno && n >= 0 && !*en) *w = W_REL - n;
		} else {
			int p = get_width(attr, "width", sh);

			if (p >= 0) *w = p;
		}
		mem_free(al);
	}
}

static struct table *
new_table(void)
{
	struct table *t = mem_calloc(1, sizeof(struct table));

	if (!t) return NULL;

	t->rx = INIT_X;
	t->ry = INIT_Y;

	t->cells = mem_calloc(INIT_X * INIT_Y, sizeof(struct table_cell));
	if (!t->cells) {
		mem_free(t);
		return NULL;
	}

	t->rc = INIT_X;

	t->cols = mem_calloc(INIT_X, sizeof(struct table_column));
	if (!t->cols) {
		mem_free(t->cells);
		mem_free(t);
		return NULL;
	}

	return t;
}

static void
free_table(struct table *t)
{
	if (t->min_c) mem_free(t->min_c);
	if (t->max_c) mem_free(t->max_c);
	if (t->w_c) mem_free(t->w_c);
	if (t->r_heights) mem_free(t->r_heights);
	mem_free(t->cols);
	if (t->xcols) mem_free(t->xcols);
	mem_free(t->cells);
	mem_free(t);
}

static void
expand_cells(struct table *t, int x, int y)
{
	if (x >= t->x) {
		if (t->x) {
			int tx = t->x - 1;
			register int i;

			for (i = 0; i < t->y; i++) {
				register int j;
				struct table_cell *cellp = CELL(t, tx, i);

				if (cellp->colspan != -1) continue;

				for (j = t->x; j <= x; j++) {
					struct table_cell *cell = CELL(t, j, i);

					cell->used = 1;
					cell->spanned = 1;
					cell->rowspan = cellp->rowspan;
					cell->colspan = -1;
					cell->mx = cellp->mx;
					cell->my = cellp->my;
				}
			}
		}
		t->x = x + 1;
	}

	if (y >= t->y) {
		if (t->y) {
			int ty = t->y - 1;
			register int i;

			for (i = 0; i < t->x; i++) {
				register int j;
				struct table_cell *cellp = CELL(t, i, ty);

				if (cellp->rowspan != -1) continue;

				for (j = t->y; j <= y; j++) {
					struct table_cell *cell = CELL(t, i, j);

					cell->used = 1;
					cell->spanned = 1;
					cell->rowspan = -1;
					cell->colspan = cellp->colspan;
					cell->mx = cellp->mx;
					cell->my = cellp->my;
				}
			}
		}
		t->y = y + 1;
	}
}

static struct table_cell *
new_cell(struct table *t, int x, int y)
{
	if (x < t->x && y < t->y) return CELL(t, x, y);

	while (1) {
		struct table nt;
		register int i = 0;

		if (x < t->rx && y < t->ry) {
			expand_cells(t, x, y);
			return CELL(t, x, y);
		}

		nt.rx = t->rx;
		nt.ry = t->ry;

		while (x >= nt.rx) if (!(nt.rx <<= 1)) return NULL;
		while (y >= nt.ry) if (!(nt.ry <<= 1)) return NULL;

		nt.cells = mem_calloc(nt.rx * nt.ry, sizeof(struct table_cell));
		if (!nt.cells) return NULL;

		while (i < t->x) {
			register int j = 0;

			while (j < t->y) {
				memcpy(CELL(&nt, i, j), CELL(t, i, j),
				       sizeof(struct table_cell));
				j++;
			}
			i++;
		}

		mem_free(t->cells);
		t->cells = nt.cells;
		t->rx = nt.rx;
		t->ry = nt.ry;
	}
}

static void
new_columns(struct table *t, int span, int width, int align,
	    int valign, int group)
{
	if (t->c + span > t->rc) {
		int n = t->rc;
		struct table_column *nc;

		while (t->c + span > n) if (!(n <<= 1)) return;

		nc = mem_realloc(t->cols, n * sizeof(struct table_column));
		if (!nc) return;

		t->rc = n;
		t->cols = nc;
	}

	while (span--) {
		t->cols[t->c].align = align;
		t->cols[t->c].valign = valign;
		t->cols[t->c].width = width;
		t->cols[t->c++].group = group;
		group = 0;
	}
}

static void
set_td_width(struct table *t, int x, int width, int f)
{
	if (x >= t->xc) {
		int n = t->xc;
		register int i;
		int *nc;

		while (x >= n) if (!(n <<= 1)) break;
		if (!n && t->xc) return;
		if (!n) n = x + 1;

		nc = mem_realloc(t->xcols, n * sizeof(int));
		if (!nc) return;

		for (i = t->xc; i < n; i++) nc[i] = W_AUTO;
		t->xc = n;
		t->xcols = nc;
	}

	if (t->xcols[x] == W_AUTO || f) {
		t->xcols[x] = width;
		return;
	}

	if (width == W_AUTO) return;

	if (width < 0 && t->xcols[x] >= 0) {
		t->xcols[x] = width;
		return;
	}

	if (width >= 0 && t->xcols[x] < 0) return;
	t->xcols[x] = (t->xcols[x] + width) >> 1;
}

static unsigned char *
skip_table(unsigned char *html, unsigned char *eof)
{
	int level = 1;
	unsigned char *name;
	int namelen;

	while (1) {
		while (html < eof
		       && (*html != '<'
		           || parse_element(html, eof, &name, &namelen, NULL,
					    &html)))
			html++;

		if (html >= eof) return eof;
		if (namelen == 5 && !strncasecmp(name, "TABLE", 5)) level++;
		if (namelen == 6 && !strncasecmp(name, "/TABLE", 6)) {
			level--;
			if (!level) return html;
		}
	}
}

static struct table *
parse_table(unsigned char *html, unsigned char *eof,
	    unsigned char **end, color_t bgcolor,
	    int sh, struct s_e **bad_html, int *bhp)
{
	struct table *t;
	struct table_cell *cell;
	unsigned char *t_name, *t_attr, *en;
	unsigned char *lbhp = NULL;
	color_t l_col = bgcolor;
	int t_namelen;
	int p = 0;
	int l_al = AL_LEFT;
	int l_val = VAL_MIDDLE;
	int csp, rsp;
	int group = 0;
	int i, j, k;
	int qqq;
	int c_al = AL_TR, c_val = VAL_TR, c_width = W_AUTO, c_span = 0;
	register int x = 0, y = -1;

	*end = html;

	if (bad_html) {
		*bad_html = NULL;
		*bhp = 0;
	}
	t = new_table();
	if (!t) return NULL;

	t->bgcolor = bgcolor;
se:
	en = html;

see:
	html = en;
	if (bad_html && !p && !lbhp) {
		if (!(*bhp & (ALLOC_GR-1))) {
			struct s_e *s_e = mem_realloc(*bad_html, (*bhp + ALLOC_GR)
								 * sizeof(struct s_e));

			if (!s_e) goto qwe;
			*bad_html = s_e;
		}
		lbhp = html;
		(*bad_html)[(*bhp)++].s = html;
	}

qwe:
	while (html < eof && *html != '<') html++;

	if (html >= eof) {
		if (p) CELL(t, x, y)->end = html;
		if (lbhp) (*bad_html)[*bhp-1].e = html;
		goto scan_done;
	}

	if (html + 2 <= eof && (html[1] == '!' || html[1] == '?')) {
		html = skip_comment(html, eof);
		goto se;
	}

	if (parse_element(html, eof, &t_name, &t_namelen, &t_attr, &en)) {
		html++;
		goto se;
	}

	if (t_namelen == 5 && !strncasecmp(t_name, "TABLE", 5)) {
		en = skip_table(en, eof);
		goto see;
	}

	if (t_namelen == 6 && !strncasecmp(t_name, "/TABLE", 6)) {
		if (c_span) new_columns(t, c_span, c_width, c_al, c_val, 1);
		if (p) CELL(t, x, y)->end = html;
		if (lbhp) (*bad_html)[*bhp-1].e = html;
		goto scan_done;
	}

	if (t_namelen == 8 && !strncasecmp(t_name, "COLGROUP", 8)) {
		if (c_span) new_columns(t, c_span, c_width, c_al, c_val, 1);
		if (lbhp) {
			(*bad_html)[*bhp-1].e = html;
			lbhp = NULL;
		}
		c_al = AL_TR;
		c_val = VAL_TR;
		c_width = W_AUTO;
		get_align(t_attr, &c_al);
		get_valign(t_attr, &c_val);
		get_c_width(t_attr, &c_width, sh);
		c_span = get_num(t_attr, "span");
		if (c_span == -1) c_span = 1;
		goto see;
	}

	if (t_namelen == 9 && !strncasecmp(t_name, "/COLGROUP", 9)) {
		if (c_span) new_columns(t, c_span, c_width, c_al, c_val, 1);
		if (lbhp) {
			(*bad_html)[*bhp-1].e = html;
			lbhp = NULL;
		}
		c_span = 0;
		c_al = AL_TR;
		c_val = VAL_TR;
		c_width = W_AUTO;
		goto see;
	}

	if (t_namelen == 3 && !strncasecmp(t_name, "COL", 3)) {
		int sp, wi, al, val;

		if (lbhp) {
			(*bad_html)[*bhp-1].e = html;
			lbhp = NULL;
		}

		sp = get_num(t_attr, "span");
		if (sp == -1) sp = 1;

		wi = c_width;
		al = c_al;
		val = c_val;
		get_align(t_attr, &al);
		get_valign(t_attr, &val);
		get_c_width(t_attr, &wi, sh);
		new_columns(t, sp, wi, al, val, !!c_span);
		c_span = 0;
		goto see;
	}

	/* /TR /TD /TH */
	if (t_namelen == 3
	    && t_name[0] == '/'
	    && upcase(t_name[1]) == 'T') {
	        unsigned char c = upcase(t_name[2]);

		if (c == 'R' || c == 'D' || c == 'H') {
	 		if (c_span)
				new_columns(t, c_span, c_width, c_al, c_val, 1);

			if (p) {
				CELL(t, x, y)->end = html;
				p = 0;
			}
			if (lbhp) {
				(*bad_html)[*bhp-1].e = html;
				lbhp = NULL;
			}
		}
	}

	/* All following tags have T as first letter. */
	if (upcase(t_name[0]) != 'T') goto see;

	/* TR */
	if (t_namelen == 2 && upcase(t_name[1]) == 'R') {
		if (c_span) new_columns(t, c_span, c_width, c_al, c_val, 1);

		if (p) {
			CELL(t, x, y)->end = html;
			p = 0;
		}
		if (lbhp) {
			(*bad_html)[*bhp-1].e = html;
			lbhp = NULL;
		}

		if (group) group--;
		l_al = AL_LEFT;
		l_val = VAL_MIDDLE;
		l_col = bgcolor;
		get_align(t_attr, &l_al);
		get_valign(t_attr, &l_val);
		get_bgcolor(t_attr, &l_col);
		y++;
		x = 0;
		goto see;
	}

	/* THEAD TBODY TFOOT */
	if (t_namelen == 5
	    && ((!strncasecmp(&t_name[1], "HEAD", 4)) ||
		(!strncasecmp(&t_name[1], "BODY", 4)) ||
		(!strncasecmp(&t_name[1], "FOOT", 4)))) {
		if (c_span) new_columns(t, c_span, c_width, c_al, c_val, 1);

		if (lbhp) {
			(*bad_html)[*bhp-1].e = html;
			lbhp = NULL;
		}

		group = 2;
	}

	/* TD TH */
	if (t_namelen != 2
	    || (upcase(t_name[1]) != 'D'
		&& upcase(t_name[1]) != 'H'))
		goto see;

	if (c_span) new_columns(t, c_span, c_width, c_al, c_val, 1);

	if (lbhp) {
		(*bad_html)[*bhp-1].e = html;
		lbhp = NULL;
	}
	if (p) {
		CELL(t, x, y)->end = html;
		p = 0;
	}

	if (y == -1) {
		y = 0;
		x = 0;
	}

nc:
	cell = new_cell(t, x, y);
	if (!cell) goto see;

	if (cell->used) {
		if (cell->colspan == -1) goto see;
		x++;
		goto nc;
	}

	p = 1;

	cell->mx = x;
	cell->my = y;
	cell->used = 1;
	cell->start = en;

	cell->align = l_al;
	cell->valign = l_val;

	cell->b = (upcase(t_name[1]) == 'H');
	if (cell->b) cell->align = AL_CENTER;

	if (group == 1) cell->group = 1;

	if (x < t->c) {
		if (t->cols[x].align != AL_TR)
			cell->align = t->cols[x].align;
		if (t->cols[x].valign != VAL_TR)
			cell->valign = t->cols[x].valign;
	}

	cell->bgcolor = l_col;

	get_align(t_attr, &cell->align);
	get_valign(t_attr, &cell->valign);
	get_bgcolor(t_attr, &cell->bgcolor);

	csp = get_num(t_attr, "colspan");
	if (csp == -1) csp = 1;
	else if (!csp) csp = -1;

	rsp = get_num(t_attr, "rowspan");
	if (rsp == -1) rsp = 1;
	else if (!rsp) rsp = -1;

	cell->colspan = csp;
	cell->rowspan = rsp;

	if (csp == 1) {
		int w = W_AUTO;

		get_c_width(t_attr, &w, sh);
		if (w != W_AUTO) set_td_width(t, x, w, 0);
	}

	qqq = t->x;

	for (i = 1; csp != -1 ? i < csp : i < qqq; i++) {
		struct table_cell *sc = new_cell(t, x + i, y);

		if (!sc || sc->used) {
			csp = i;
			for (k = 0; k < i; k++) CELL(t, x + k, y)->colspan = csp;
			break;
		}

		sc->used = sc->spanned = 1;
		sc->rowspan = rsp;
		sc->colspan = csp;
		sc->mx = x;
		sc->my = y;
	}

	qqq = t->y;
	for (j = 1; rsp != -1 ? j < rsp : j < qqq; j++) {
		for (k = 0; k < i; k++) {
			struct table_cell *sc = new_cell(t, x + k, y + j);

			if (!sc || sc->used) {
				int l, m;

				if (sc->mx == x && sc->my == y) continue;

				/* internal("boo"); */

				for (l = 0; l < k; l++)
					memset(CELL(t, x + l, y + j), 0,
					       sizeof(struct table_cell));

				rsp = j;

				for (l = 0; l < i; l++)
					for (m = 0; m < j; m++)
						CELL(t, x + l, y + m)->rowspan = j;
				goto see;
			}

			sc->used = sc->spanned = 1;
			sc->rowspan = rsp;
			sc->colspan = csp;
			sc->mx = x;
			sc->my = y;
		}
	}

	goto see;

scan_done:
	*end = html;

	for (x = 0; x < t->x; x++) for (y = 0; y < t->y; y++) {
		struct table_cell *c = CELL(t, x, y);

		if (!c->spanned) {
			if (c->colspan == -1) c->colspan = t->x - x;
			if (c->rowspan == -1) c->rowspan = t->y - y;
		}
	}

	if (t->y) {
		t->r_heights = mem_calloc(t->y, sizeof(int));
		if (!t->r_heights) {
			free_table(t);
			return NULL;
		}
	} else t->r_heights = NULL;

	for (x = 0; x < t->c; x++)
		if (t->cols[x].width != W_AUTO)
			set_td_width(t, x, t->cols[x].width, 1);
	set_td_width(t, t->x, W_AUTO, 0);

	return t;
}

static inline void
get_cell_width(unsigned char *start, unsigned char *end, int cellpd, int w,
	       int a, int *min, int *max, int n_link, int *n_links)
{
	struct part *p;

	if (min) *min = -1;
	if (max) *max = -1;
	if (n_links) *n_links = n_link;

	p = format_html_part(start, end, AL_LEFT, cellpd, w, NULL, !!a, !!a,
			     NULL, n_link);
	if (!p) return;

	if (min) *min = p->x;
	if (max) *max = p->xmax;
	if (n_links) *n_links = p->link_num;

	assertm(!((min && max && *min > *max)), "get_cell_width: %d > %d",
		*min, *max);

	mem_free(p);
}

static inline void
check_cell_widths(struct table *t)
{
	register int i, j;

	for (j = 0; j < t->y; j++) for (i = 0; i < t->x; i++) {
		int min, max;
		struct table_cell *c = CELL(t, i, j);

		if (!c->start) continue;

		get_cell_width(c->start, c->end, t->cellpd, 0, 0,
			       &min, &max, c->link_num, NULL);

		assertm(!(min != c->min_width || max < c->max_width),
			"check_cell_widths failed");
	}
}

static inline void
get_cell_widths(struct table *t)
{
	int nl = t->p->link_num;
	register int i, j;

	if (!d_opt->table_order)
		for (j = 0; j < t->y; j++)
			for (i = 0; i < t->x; i++) {
				struct table_cell *c = CELL(t, i, j);

				if (!c->start) continue;
				c->link_num = nl;
				get_cell_width(c->start, c->end, t->cellpd, 0, 0,
					       &c->min_width, &c->max_width, nl, &nl);
			}
	else
		for (i = 0; i < t->x; i++)
			for (j = 0; j < t->y; j++) {
				struct table_cell *c = CELL(t, i, j);

				if (!c->start) continue;
				c->link_num = nl;
				get_cell_width(c->start, c->end, t->cellpd, 0, 0,
					       &c->min_width, &c->max_width, nl, &nl);
			}

	t->link_num = nl;
}

static inline void
dst_width(int *p, int n, int w, int *lim)
{
	register int i;
	int s = 0, d, r, t;

	for (i = 0; i < n; i++) s += p[i];
	if (s >= w) return;

again:
	t = w - s;
	d = t / n;
	r = t % n;
	w = 0;

	if (lim) {
		for (i = 0; i < n; i++) {
			p[i] += d + (i < r);
			if (p[i] > lim[i]) {
				w += p[i] - lim[i];
				p[i] = lim[i];
			}
		}
	} else {
		for (i = 0; i < n; i++) {
			p[i] += d + (i < r);
		}
	}

	if (w) {
		assertm(lim, "bug in dst_width");
		lim = NULL;
		s = 0;
		goto again;
	}
}


/* Returns: -1 none, 0, space, 1 line, 2 double */
static inline int
get_vline_width(struct table *t, int col)
{
	int w = 0;

	if (!col) return -1;

	if (t->rules == R_COLS || t->rules == R_ALL)
		w = t->cellsp;
	else if (t->rules == R_GROUPS)
		w = (col < t->c && t->cols[col].group);

	if (!w && t->cellpd) w = -1;

	return w;
}

static int
get_hline_width(struct table *t, int row)
{
	int w = 0;

	if (!row) return -1;

	if (t->rules == R_ROWS || t->rules == R_ALL) {

x:
		if (t->cellsp || t->vcellpd) return t->cellsp;
		return -1;

	} else if (t->rules == R_GROUPS) {
		register int q;

		for (q = 0; q < t->x; q++)
			if (CELL(t, q, row)->group)
				goto x;
		return t->vcellpd ? 0 : -1;
	}

	if (!w && !t->vcellpd) w = -1;

	return w;
}

static int
get_column_widths(struct table *t)
{
	int s = 1;

	if (!t->x) return -1; /* prevents calloc(0, sizeof(int)) calls */

	if (!t->min_c) {
		t->min_c = mem_calloc(t->x, sizeof(int));
		if (!t->min_c) return -1;
	}

	if (!t->max_c) {
		t->max_c = mem_calloc(t->x, sizeof(int));
	   	if (!t->max_c) {
			mem_free(t->min_c), t->min_c = NULL;
			return -1;
		}
	}

	if (!t->w_c) {
		t->w_c = mem_calloc(t->x, sizeof(int));
		if (!t->w_c) {
			mem_free(t->min_c), t->min_c = NULL;
			mem_free(t->max_c), t->max_c = NULL;
			return -1;
		}
	}

	do {
		register int i = 0, j;
		int ns = MAXINT;

		for (; i < t->x; i++) for (j = 0; j < t->y; j++) {
			struct table_cell *c = CELL(t, i, j);

			if (c->spanned || !c->used) continue;

			assertm(c->colspan + i <= t->x, "colspan out of table");
			if_assert_failed return -1;

			if (c->colspan == s) {
				register int k, p = 0;

				for (k = 1; k < s; k++)
					p += (get_vline_width(t, i + k) >= 0);

				dst_width(t->min_c + i, s,
				  	  c->min_width - p,
					  t->max_c + i);

				dst_width(t->max_c + i, s,
				  	  c->max_width - p,
					  NULL);

				for (k = 0; k < s; k++) {
					int tmp = i + k;

					t->max_c[tmp] = int_max(t->max_c[tmp], t->min_c[tmp]);
				}

			} else if (c->colspan > s && c->colspan < ns) {
				ns = c->colspan;
			}
		}
		s = ns;
	} while (s != MAXINT);

	return 0;
}

static void
get_table_width(struct table *t)
{
	int min = 0;
	int max = 0;
	register int i = 0;

	while (i < t->x) {
		int vl = (get_vline_width(t, i) >= 0);

		min += vl + t->min_c[i];
		max += vl + t->max_c[i];
		if (t->xcols[i] > t->max_c[i])
			max += t->xcols[i];
		i++;
	}

	if (t->border) {
		if (t->frame & F_LHS) {
			min++;
			max++;
		}
		if (t->frame & F_RHS) {
			min++;
			max++;
		}
	}

	t->min_t = min;
	t->max_t = max;
	assertm(min <= max, "min(%d) > max(%d)", min, max);
	/* XXX: Recovery path? --pasky */
}


/* TODO: understand and rewrite this thing... --Zas */
static void
distribute_widths(struct table *t, int width)
{
	register int i;
	int d = width - t->min_t;
	int om = 0;
	char *u;
	int *w, *mx;
	int mmax_c = 0;
	int tx_size;

	if (!t->x) return;

	assertm(d >= 0, "too small width %d, required %d", width, t->min_t);

	for (i = 0; i < t->x; i++)
		mmax_c = int_max(mmax_c, t->max_c[i]);

	tx_size = t->x * sizeof(int);
	memcpy(t->w_c, t->min_c, tx_size);
	t->rw = width;

	/* XXX: We don't need to fail if unsuccessful.  See below. --Zas */
	u = fmem_alloc(t->x);

	w = fmem_alloc(tx_size);
	if (!w) goto end;

	mx = fmem_alloc(tx_size);
	if (!mx) goto end1;

	while (d) {
		int mss, mii;
		int p = 0;
		int wq;
		int dd;

		memset(w, 0, tx_size);
		memset(mx, 0, tx_size);

		for (i = 0; i < t->x; i++) {
			switch (om) {
				case 0:
					if (t->w_c[i] < t->xcols[i]) {
						w[i] = 1;
						if (t->xcols[i] > t->max_c[i]) {
							mx[i] = t->max_c[i];
						} else {
							mx[i] = t->xcols[i];
						}
						mx[i] -= t->w_c[i];
						if (mx[i] <= 0) w[i] = 0;
					}

					break;
				case 1:
					if (t->xcols[i] < -1 && t->xcols[i] != -2) {
						if (t->xcols[i] <= -2) {
							w[i] = -2 - t->xcols[i];
						} else {
							w[i] = 1;
						}
						mx[i] = t->max_c[i] - t->w_c[i];
						if (mx[i] <= 0) w[i] = 0;
					}
					break;
				case 2:
				case 3:
					if (t->w_c[i] < t->max_c[i]
					    && (om == 3 || t->xcols[i] == W_AUTO)) {
						mx[i] = t->max_c[i] - t->w_c[i];
						if (mmax_c) {
							w[i] = 5 + t->max_c[i] * 10 / mmax_c;
						} else {
							w[i] = 1;
						}
					}
					break;
				case 4:
					if (t->xcols[i] >= 0) {
						w[i] = 1;
						mx[i] = t->xcols[i] - t->w_c[i];
						if (mx[i] <= 0) w[i] = 0;
					}
					break;
				case 5:
					if (t->xcols[i] < 0) {
						if (t->xcols[i] <= -2) {
							w[i] =  -2 - t->xcols[i];
						} else {
							w[i] = 1;
						}
						mx[i] = MAXINT;
					}
					break;
				case 6:
					w[i] = 1;
					mx[i] = MAXINT;
					break;
				default:
					internal("could not expand table");
					goto end2;
			}
			p += w[i];
		}

		if (!p) {
			om++;
			continue;
		}

		wq = 0;
		if (u) memset(u, 0, t->x);
		dd = d;

a:
		mss = 0;
		mii = -1;
		for (i = 0; i < t->x; i++) if (w[i]) {
			int ss;

			if (u && u[i]) continue;
			ss = dd * w[i] / p;
			if (!ss) ss = 1;
			if (ss > mx[i]) ss = mx[i];
			if (ss > mss) {
				mss = ss;
				mii = i;
			}
		}

		if (mii != -1) {
			int q = t->w_c[mii];

			if (u) u[mii] = 1;
			t->w_c[mii] += mss;
			d -= t->w_c[mii] - q;
			while (d < 0) {
				t->w_c[mii]--;
				d++;
			}
			assertm(t->w_c[mii] >= q, "shrinking cell");
			wq = 1;
			if (d) goto a;
		} else if (!wq) om++;
	}

end2:
	fmem_free(mx);

end1:
	fmem_free(w);

end:
	if (u) fmem_free(u);
}



#ifdef HTML_TABLE_2ND_PASS /* This is by default ON! (<setup.h>) */
static void
check_table_widths(struct table *t)
{
	register int i, j;
	int s, ns;
	int m, mi = 0; /* go away, warning! */
	int *w = mem_calloc(t->x, sizeof(int));

	if (!w) return;

	for (j = 0; j < t->y; j++) for (i = 0; i < t->x; i++) {
		struct table_cell *c = CELL(t, i, j);
		register int k, p = 0;

		if (!c->start) continue;

		for (k = 0; k < c->colspan; k++) {
			p += t->w_c[i + k] +
			     (k && get_vline_width(t, i + k) >= 0);
		}

		get_cell_width(c->start, c->end, t->cellpd, p, 1, &c->x_width,
			       NULL, c->link_num, NULL);

		int_upper_bound(&c->x_width, p);
	}

	s = 1;
	do {
		ns = MAXINT;
		for (i = 0; i < t->x; i++) for (j = 0; j < t->y; j++) {
			struct table_cell *c = CELL(t, i, j);

			if (!c->start) continue;

			assertm(c->colspan + i <= t->x, "colspan out of table");
			if_assert_failed goto end;

			if (c->colspan == s) {
				int k, p = 0;

				for (k = 1; k < s; k++)
					p += (get_vline_width(t, i + k) >= 0);

				dst_width(w + i, s, c->x_width - p, t->max_c + i);

			} else if (c->colspan > s && c->colspan < ns) {
				ns = c->colspan;
			}
		}
		s = ns;
	} while (s != MAXINT);

	s = ns = 0;
	for (i = 0; i < t->x; i++) {
		s += t->w_c[i];
		ns += w[i];
	}

	if (ns > s) {
		/* internal("new width(%d) is larger than previous(%d)", ns, s); */
		goto end;
	}

	m = -1;
	for (i = 0; i < t->x; i++)
		if (t->max_c[i] > m) {
			m = t->max_c[i];
			mi = i;
		}

	if (m != -1) {
		w[mi] += s - ns;
		if (w[mi] <= t->max_c[mi]) {
			mem_free(t->w_c);
			t->w_c = w;
			return;
		}
	}

end:
	mem_free(w);
}
#endif

static void
get_table_heights(struct table *t)
{
	int s = 1;
	register int i, j;

	for (j = 0; j < t->y; j++) {
		for (i = 0; i < t->x; i++) {
			struct table_cell *cell = CELL(t, i, j);
			struct part *p;
			int xw = 0, sp;

			if (!cell->used || cell->spanned) continue;

			for (sp = 0; sp < cell->colspan; sp++) {
				xw += t->w_c[i + sp] +
				      (sp < cell->colspan - 1 &&
				       get_vline_width(t, i + sp + 1) >= 0);
			}

			p = format_html_part(cell->start, cell->end,
					     cell->align, t->cellpd, xw, NULL,
					     2, 2, NULL, cell->link_num);
			if (!p) return;

			cell->height = p->y;
			/* debug("%d, %d.",xw, cell->height); */
			mem_free(p);
		}
	}

	do {
		int ns = MAXINT;

		for (j = 0; j < t->y; j++) {
			for (i = 0; i < t->x; i++) {
				struct table_cell *cell = CELL(t, i, j);

				if (!cell->used || cell->spanned) continue;

				if (cell->rowspan == s) {
					register int k, p = 0;

					for (k = 1; k < s; k++)
						p += (get_hline_width(t, j + k) >= 0);

					dst_width(t->r_heights + j, s,
						  cell->height - p, NULL);

				} else if (cell->rowspan > s &&
					   cell->rowspan < ns) {
					ns = cell->rowspan;
				}

			}
		}
		s = ns;
	} while (s != MAXINT);

	t->rh = 0;
	if (t->border) {
		if (t->frame & F_ABOVE) t->rh++;
		if (t->frame & F_BELOW) t->rh++;
	}

	for (j = 0; j < t->y; j++) {
		t->rh += t->r_heights[j] +
			 (j && get_hline_width(t, j) >= 0);
	}
}

static void
display_complicated_table(struct table *t, int x, int y, int *yy)
{
	register int i, j;
	struct document *f = t->p->document;
	int yp;
	int xp = x + (t->border && (t->frame & F_LHS));

	for (i = 0; i < t->x; i++) {
		yp = y + (t->border && (t->frame & F_ABOVE));
		for (j = 0; j < t->y; j++) {
			struct table_cell *cell = CELL(t, i, j);

			if (cell->start) {
				struct part *p = NULL;
				int xw = 0;
				int yw = 0;
				register int s;

				for (s = 0; s < cell->colspan; s++) {
					xw += t->w_c[i + s] +
					      (s < cell->colspan - 1 &&
					       get_vline_width(t, i + s + 1) >= 0);
				}

				for (s = 0; s < cell->rowspan; s++) {
					yw += t->r_heights[j + s] +
					      (s < cell->rowspan - 1 &&
					       get_hline_width(t, j + s + 1) >= 0);
				}

				par_format.bgcolor = t->bgcolor;
				for (s = yp; s < yp + yw; s++) {
					expand_lines(t->p, s);
					expand_line(t->p, s, xp);
				}

				html_stack_dup();
				html_top.dontkill = 1;

				if (cell->b) format.attr |= AT_BOLD;

				format.bg = cell->bgcolor;
				par_format.bgcolor = cell->bgcolor;
 				{
					int tmpy = t->p->yp + yp;

					if (cell->valign == VAL_MIDDLE)
						tmpy += (yw - cell->height)>>1;
					else if (cell->valign == VAL_BOTTOM)
						tmpy += (yw - cell->height);

				   	p = format_html_part(cell->start,
							     cell->end,
							     cell->align,
							     t->cellpd, xw, f,
							     t->p->xp + xp,
							     tmpy, NULL,
							     cell->link_num);
				}

				if (p) {
					int yt;

					for (yt = 0; yt < p->y; yt++) {
						expand_lines(t->p, yp + yt);
						expand_line(t->p, yp + yt, xp + t->w_c[i]);
					}
					mem_free(p);
				}

				kill_html_stack_item(&html_top);
			}

			yp += t->r_heights[j] +
			      (j < t->y - 1 && get_hline_width(t, j + 1) >= 0);
		}

		if (i < t->x - 1) {
			xp += t->w_c[i] + (get_vline_width(t, j + 1) >= 0);
		}
	}

	yp = y;
	for (j = 0; j < t->y; j++) {
		yp += t->r_heights[j] +
		      (j < t->y - 1 && get_hline_width(t, j + 1) >= 0);
	}

	*yy = yp;
	if (t->border) {
		if (t->frame & F_ABOVE) (*yy)++;
		if (t->frame & F_BELOW) (*yy)++;
	}
}


#ifndef DEBUG
#define H_LINE_X(term, xx, yy) frame[0][(xx) + 1 + ((term)->x + 2) * (yy)]
#define V_LINE_X(term, xx, yy) frame[1][(yy) + 1 + ((term)->y + 2) * (xx)]
#else
#define H_LINE_X(term, xx, yy) (*(xx < -1 || xx > (term)->x + 1 || yy < 0 || yy > (term)->y ? \
		   	(signed char *) NULL : &frame[0][(xx) + 1 + ((term)->x + 2) * (yy)]))
#define V_LINE_X(term, xx, yy) (*(xx < 0 || xx > (term)->x || yy < -1 || yy > (term)->y + 1 ? \
			(signed char *) NULL : &frame[1][(yy) + 1 + ((term)->y + 2) * (xx)]))
#endif

#define H_LINE(term, xx, yy) int_max(H_LINE_X(term, (xx), (yy)), 0)
#define V_LINE(term, xx, yy) int_max(V_LINE_X(term, (xx), (yy)), 0)

static inline void
draw_frame_point(struct table *table, signed char *frame[2], int x, int y,
		 int i, int j)
{
	if (H_LINE_X(table, i - 1, j) >= 0
	    || H_LINE_X(table, i, j) >= 0
	    || V_LINE_X(table, i, j - 1) >= 0
	    || V_LINE_X(table, i, j) >= 0) {
		register int pos = V_LINE(table, i, j - 1)
				 + 3 * H_LINE(table, i, j)
				 + 9 * H_LINE(table, i - 1, j)
				 + 27 * V_LINE(table, i, j);

		xset_hchar(table->p, x, y, frame_table[pos],
			   par_format.bgcolor, SCREEN_ATTR_FRAME);
	}
}

static inline void
draw_frame_hline(struct table *table, signed char *frame[2], int x, int y,
		 int i, int j)
{
	if (H_LINE_X(table, i, j) >= 0) {
		xset_hchars(table->p, x, y, table->w_c[i],
			    hline_table[H_LINE(table, i, j)],
			    par_format.bgcolor, SCREEN_ATTR_FRAME);
	}
}

static inline void
draw_frame_vline(struct table *table, signed char *frame[2], int x, int y,
		 int i, int j)
{
	if (V_LINE_X(table, i, j) >= 0) {
		xset_vchars(table->p, x, y, table->r_heights[j],
			    vline_table[V_LINE(table, i, j)],
			    par_format.bgcolor, SCREEN_ATTR_FRAME);
	}
}

static void
display_table_frames(struct table *t, int x, int y)
{
 	signed char *frame[2];
  	register int i, j;
  	int cx, cy;
  	int fa = 0, fb = 0, fl = 0, fr = 0;
  	int fh_size = (t->x + 2) * (t->y + 1);
  	int fv_size = (t->x + 1) * (t->y + 2);
  
 	frame[0] = fmem_alloc(fh_size + fv_size);
 	if (!frame[0]) return;
 	memset(frame[0], -1, fh_size + fv_size);
 
 	frame[1] = &frame[0][fh_size];

	if (t->rules == R_NONE) goto cont2;

	for (j = 0; j < t->y; j++) for (i = 0; i < t->x; i++) {
		int xsp, ysp;
		struct table_cell *cell = CELL(t, i, j);

		if (!cell->used || cell->spanned) continue;
		xsp = cell->colspan;
		if (!xsp) xsp = t->x - i;
		ysp = cell->rowspan;
		if (!ysp) ysp = t->y - j;

		if (t->rules != R_COLS) {
			register int lx;

			for (lx = 0; lx < xsp; lx++) {
				H_LINE_X(t, i + lx, j) = t->cellsp;
				H_LINE_X(t, i + lx, j + ysp) = t->cellsp;
			}
		}

		if (t->rules != R_ROWS) {
			register int ly;

			for (ly = 0; ly < ysp; ly++) {
				V_LINE_X(t, i, j + ly) = t->cellsp;
				V_LINE_X(t, i + xsp, j + ly) = t->cellsp;
			}
		}
	}

	if (t->rules == R_GROUPS) {
		for (i = 1; i < t->x; i++) {
			if (/*i < t->xc &&*/ t->xcols[i]) continue;
			for (j = 0; j < t->y; j++) V_LINE_X(t, i, j) = 0;
		}
		for (j = 1; j < t->y; j++) {
			for (i = 0; i < t->x; i++)
				if (CELL(t, i, j)->group)
					goto cont;
			for (i = 0; i < t->x; i++)
				H_LINE_X(t, i, j) = 0;
cont:;
		}
	}

cont2:
	if (t->border) {
		fa = !!(t->frame & F_ABOVE);
		fb = !!(t->frame & F_BELOW);
		fl = !!(t->frame & F_LHS);
		fr = !!(t->frame & F_RHS);
	}

	for (i = 0; i < t->x; i++) {
		H_LINE_X(t, i, 0) = fa;
		H_LINE_X(t, i, t->y) = fb;
	}

	for (j = 0; j < t->y; j++) {
		V_LINE_X(t, 0, j) = fl;
		V_LINE_X(t, t->x, j) = fr;
	}

	cy = y;
	for (j = 0; j <= t->y; j++) {
		cx = x;
		if ((j > 0 && j < t->y && get_hline_width(t, j) >= 0)
		    || (j == 0 && fa)
		    || (j == t->y && fb)) {
			int w = fl ? t->border : -1;

			for (i = 0; i < t->x; i++) {
				if (i > 0)
					w = get_vline_width(t, i);

				if (w >= 0) {
					draw_frame_point(t, frame, cx, cy, i, j);
					if (j < t->y)
						draw_frame_vline(t, frame, cx, cy + 1, i, j);
					cx++;
				}

				draw_frame_hline(t, frame, cx, cy, i, j);
				cx += t->w_c[i];
			}

			if (fr) {
				draw_frame_point(t, frame, cx, cy, i, j);
				if (j < t->y)
					draw_frame_vline(t, frame, cx, cy + 1, i, j);
				cx++;
			}

			cy++;

		} else if (j < t->y) {
			for (i = 0; i <= t->x; i++) {
				if ((i > 0 && i < t->x && get_vline_width(t, i) >= 0)
				    || (i == 0 && fl)
				    || (i == t->x && fr)) {
					draw_frame_vline(t, frame, cx, cy, i, j);
					cx++;
				}
				if (i < t->x) cx += t->w_c[i];
			}
		}

		if (j < t->y) cy += t->r_heights[j];
		/*for (cyy = cy1; cyy < cy; cyy++) expand_line(t->p, cyy, cx - 1);*/
	}

	fmem_free(frame[0]);
}

void
format_table(unsigned char *attr, unsigned char *html, unsigned char *eof,
	     unsigned char **end, void *f)
{
	struct part *p = f;
	struct table *t;
	struct s_e *bad_html;
	struct node *n, *nn;
	unsigned char *al;
	color_t bgcolor = par_format.bgcolor;
	int border, cellsp, vcellpd, cellpd, align;
	int frame, rules, width, wf;
	int cye;
	int x;
	int i;
	int bad_html_n;
	int cpd_pass, cpd_width, cpd_last;
	int margins;

	table_level++;
	get_bgcolor(attr, &bgcolor);

	/* From http://www.w3.org/TR/html4/struct/tables.html#adef-border-TABLE
	 * The following settings should be observed by user agents for
	 * backwards compatibility.
	 * Setting border="0" implies frame="void" and, unless otherwise
	 * specified, rules="none".
	 * Other values of border imply frame="border" and, unless otherwise
	 * specified, rules="all".
	 * The value "border" in the start tag of the TABLE element should be
	 * interpreted as the value of the frame attribute. It implies
	 * rules="all" and some default (non-zero) value for the border
	 * attribute. */
	border = get_num(attr, "border");
 	if (border == -1) {
		border = has_attr(attr, "border")
			 || has_attr(attr, "rules")
			 || has_attr(attr, "frame");
	}

	if (border) {
		int_upper_bound(&border, 2);

		cellsp = get_num(attr, "cellspacing");
		int_bounds(&cellsp, 1, 2);

		frame = F_BOX;
		al = get_attr_val(attr, "frame");
		if (al) {
			if (!strcasecmp(al, "void")) frame = F_VOID;
			else if (!strcasecmp(al, "above")) frame = F_ABOVE;
			else if (!strcasecmp(al, "below")) frame = F_BELOW;
			else if (!strcasecmp(al, "hsides")) frame = F_HSIDES;
			else if (!strcasecmp(al, "vsides")) frame = F_VSIDES;
			else if (!strcasecmp(al, "lhs")) frame = F_LHS;
			else if (!strcasecmp(al, "rhs")) frame = F_RHS;
			else if (!strcasecmp(al, "box")) frame = F_BOX;
			else if (!strcasecmp(al, "border")) frame = F_BOX;
			mem_free(al);
		}
	} else {
		cellsp = 0;
		frame = F_VOID;
	}

	cellpd = get_num(attr, "cellpadding");
	if (cellpd == -1) {
		vcellpd = 0;
		cellpd = !!border;
	} else {
		vcellpd = (cellpd >= HTML_CHAR_HEIGHT / 2 + 1);
		cellpd = (cellpd >= HTML_CHAR_WIDTH / 2 + 1);
	}

	align = par_format.align;
	if (align == AL_NONE || align == AL_BLOCK) align = AL_LEFT;

	al = get_attr_val(attr, "align");
	if (al) {
		if (!strcasecmp(al, "left")) align = AL_LEFT;
		else if (!strcasecmp(al, "center")) align = AL_CENTER;
		else if (!strcasecmp(al, "right")) align = AL_RIGHT;
		mem_free(al);
	}

	rules = border ? R_ALL : R_NONE;
	al = get_attr_val(attr, "rules");
	if (al) {
		if (!strcasecmp(al, "none")) rules = R_NONE;
		else if (!strcasecmp(al, "groups")) rules = R_GROUPS;
		else if (!strcasecmp(al, "rows")) rules = R_ROWS;
		else if (!strcasecmp(al, "cols")) rules = R_COLS;
		else if (!strcasecmp(al, "all")) rules = R_ALL;
		mem_free(al);
	}

	wf = 0;
	width = get_width(attr, "width", (p->document || p->xp));
	if (width == -1) {
		width = par_format.width - par_format.leftmargin - par_format.rightmargin;
		if (width < 0) width = 0;
		wf = 1;
	}

	t = parse_table(html, eof, end, bgcolor, (p->document || p->xp), &bad_html, &bad_html_n);
	if (!t) {
		if (bad_html) mem_free(bad_html);
		goto ret0;
	}

	for (i = 0; i < bad_html_n; i++) {
		while (bad_html[i].s < bad_html[i].e && WHITECHAR(*bad_html[i].s))
			bad_html[i].s++;
		while (bad_html[i].s < bad_html[i].e && WHITECHAR(bad_html[i].e[-1]))
			bad_html[i].e--;
		if (bad_html[i].s < bad_html[i].e)
			parse_html(bad_html[i].s, bad_html[i].e, put_chars_f, line_break_f,
				   init_f, special_f, p, NULL);
	}

	if (bad_html) mem_free(bad_html);
	html_stack_dup();
	html_top.dontkill = 1;
	par_format.align = AL_LEFT;
	t->p = p;
	t->border = border;
	t->cellpd = cellpd;
	t->vcellpd = vcellpd;
	t->cellsp = cellsp;
	t->frame = frame;
	t->rules = rules;
	t->width = width;
	t->wf = wf;

	cpd_pass = 0;
	cpd_last = t->cellpd;
	cpd_width = 0;  /* not needed, but let the warning go away */

again:
	get_cell_widths(t);
	if (get_column_widths(t)) goto ret2;

	get_table_width(t);

	margins = par_format.leftmargin + par_format.rightmargin;
	if (!p->document && !p->xp) {
		if (!wf) int_upper_bound(&t->max_t, width);
		int_lower_bound(&t->max_t, t->min_t);

		p->xmax = int_max(p->xmax, t->max_t + margins);
		p->x = int_max(p->x, t->min_t + margins);

		goto ret2;
	}

	if (!cpd_pass && t->min_t > width && t->cellpd) {
		t->cellpd = 0;
		cpd_pass = 1;
		cpd_width = t->min_t;
		goto again;
	}
	if (cpd_pass == 1 && t->min_t > cpd_width) {
		t->cellpd = cpd_last;
		cpd_pass = 2;
		goto again;
	}

	/* debug("%d %d %d", t->min_t, t->max_t, width); */
	if (t->min_t >= width)
		distribute_widths(t, t->min_t);
	else if (t->max_t < width && wf)
		distribute_widths(t, t->max_t);
	else
		distribute_widths(t, width);

	if (!p->document && p->xp == 1) {
		int ww = t->rw + margins;

		int_bounds(&ww, t->rw, par_format.width);
		p->x = int_max(p->x, ww);
		p->cy += t->rh;

		goto ret2;
	}

#ifdef HTML_TABLE_2ND_PASS
	check_table_widths(t);
#endif

	{
		int ww = par_format.width - t->rw;

		if (align == AL_CENTER)
			x = (ww + par_format.leftmargin
		     	     - par_format.rightmargin) >> 1;
		else if (align == AL_RIGHT)
			x = ww - par_format.rightmargin;
		else
			x = par_format.leftmargin;

		if (x > ww) x = ww;
		if (x < 0) x = 0;
	}

	get_table_heights(t);

	if (!p->document) {
		p->x = int_max(p->x, t->rw + margins);
		p->cy += t->rh;
		goto ret2;
	}

	n = p->document->nodes.next;
	n->yw = p->yp - n->y + p->cy;

	display_complicated_table(t, x, p->cy, &cye);
	display_table_frames(t, x, p->cy);

	nn = mem_alloc(sizeof(struct node));
	if (nn) {
		nn->x = n->x;
		nn->y = p->yp + cye;
		nn->xw = n->xw;
		add_to_list(p->document->nodes, nn);
	}

	assertm(p->cy + t->rh == cye, "size does not match; 1:%d, 2:%d",
		p->cy + t->rh, cye);

	p->cy = cye;
	p->cx = -1;

ret2:
	p->link_num = t->link_num;
	p->y = int_max(p->y, p->cy);
	free_table(t);
	kill_html_stack_item(&html_top);

ret0:
	table_level--;
	if (!table_level) free_table_cache();
}
