/* HTML tables renderer */
/* $Id: tables.c,v 1.146 2004/03/28 18:21:39 zas Exp $ */

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
#include "document/options.h"
#include "terminal/draw.h"
#include "util/color.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memory.h"
#include "util/string.h"

/* Unsafe macros */
#include "document/html/internal.h"


/* Fix namespace clash on MacOS. */
#define table table_elinks

#define AL_TR	-1

#define VALIGN_TR	-1
#define VALIGN_TOP	0
#define VALIGN_MIDDLE	1
#define VALIGN_BOTTOM	2
#define VALIGN_BASELINE	VALIGN_TOP /* Not implemented. */

#define WIDTH_AUTO		-1
#define WIDTH_RELATIVE		-2

#define TABLE_FRAME_VOID	0
#define TABLE_FRAME_ABOVE	1
#define TABLE_FRAME_BELOW	2
#define TABLE_FRAME_HSIDES	3
#define TABLE_FRAME_LHS		4
#define TABLE_FRAME_RHS		8
#define TABLE_FRAME_VSIDES	12
#define TABLE_FRAME_BOX		15

#define TABLE_RULE_NONE		0
#define TABLE_RULE_ROWS		1
#define TABLE_RULE_COLS		2
#define TABLE_RULE_ALL		3
#define TABLE_RULE_GROUPS	4

#define INIT_X		2
#define INIT_Y		2

#define realloc_bad_html(bad_html, size) \
	mem_align_alloc(bad_html, size, (size) + 1, sizeof(struct html_start_end), 0xFF)

#define CELL(t, x, y) (&(t)->cells[(y) * (t)->rx + (x)])

/* Types and structs */

struct table_cell {
	unsigned char *start;
	unsigned char *end;
	unsigned char *fragment_id;
	color_t bgcolor;
	int mx, my;
	int align;
	int valign;
	int group;
	int colspan;
	int rowspan;
	int min_width;
	int max_width;
	int width, height;
	int link_num;

	unsigned int is_used:1;
	unsigned int is_spanned:1;
	unsigned int is_header:1;
};

struct table_column {
	int group;
	int align;
	int valign;
	int width;
};

/* TODO: rename fields. --Zas */
struct table {
	struct part *p;
	struct table_cell *cells;
	struct table_column *columns;
	unsigned char *fragment_id;
	color_t bgcolor;
	color_t bordercolor;
	int *min_c, *max_c;
	int *columns_width;
	int *xcols;
	int *rows_height;
	int x, y;
	int rx, ry;
	int border;
	int cellpadding;
	int vcellpadding;
	int cellspacing;
	int frame, rules, width, wf;
	int rw;
	int min_t, max_t;
	int columns_count, rc;
	int xc;
	int rh;
	int link_num;
};

struct html_start_end {
	unsigned char *start, *end;
};

struct table_frames {
	int top;
	int bottom;
	int left;
	int right;
};

/* Global variables */

int table_level;

static void
get_table_frames(struct table *t, struct table_frames *result)
{
	assert(t);

	if (t->border) {
		result->top = !!(t->frame & TABLE_FRAME_ABOVE);
		result->bottom = !!(t->frame & TABLE_FRAME_BELOW);
		result->left = !!(t->frame & TABLE_FRAME_LHS);
		result->right = !!(t->frame & TABLE_FRAME_RHS);
	} else {
		memset(result, 0, sizeof(struct table_frames));
	}
}

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
		if (!(strcasecmp(al, "top"))) *a = VALIGN_TOP;
		else if (!(strcasecmp(al, "middle"))) *a = VALIGN_MIDDLE;
		else if (!(strcasecmp(al, "bottom"))) *a = VALIGN_BOTTOM;
		else if (!(strcasecmp(al, "baseline"))) *a = VALIGN_BASELINE; /* NOT IMPLEMENTED */
		mem_free(al);
	}
}

static inline void
get_column_width(unsigned char *attr, int *width, int sh)
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
			if (!errno && n >= 0 && !*en)
				*width = WIDTH_RELATIVE - n;
		} else {
			int w = get_width(attr, "width", sh);

			if (w >= 0) *width = w;
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

	t->columns = mem_calloc(INIT_X, sizeof(struct table_column));
	if (!t->columns) {
		mem_free(t->cells);
		mem_free(t);
		return NULL;
	}

	return t;
}

static void
free_table(struct table *t)
{
	int i, j;

	if (t->min_c) mem_free(t->min_c);
	if (t->max_c) mem_free(t->max_c);
	if (t->columns_width) mem_free(t->columns_width);
	if (t->rows_height) mem_free(t->rows_height);
	if (t->fragment_id) mem_free(t->fragment_id);
	if (t->xcols) mem_free(t->xcols);

	for (i = 0; i < t->x; i++) {
		for (j = 0; j < t->y; j++) {
			struct table_cell *cell = CELL(t, i, j);

			if (cell->fragment_id)
				mem_free(cell->fragment_id);
		}
	}

	mem_free(t->cells);
	mem_free(t->columns);
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

					cell->is_used = 1;
					cell->is_spanned = 1;
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

					cell->is_used = 1;
					cell->is_spanned = 1;
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
	if (t->columns_count + span > t->rc) {
		int n = t->rc;
		struct table_column *new_columns;

		while (t->columns_count + span > n) if (!(n <<= 1)) return;

		new_columns = mem_realloc(t->columns, n * sizeof(struct table_column));
		if (!new_columns) return;

		t->rc = n;
		t->columns = new_columns;
	}

	while (span--) {
		t->columns[t->columns_count].align = align;
		t->columns[t->columns_count].valign = valign;
		t->columns[t->columns_count].width = width;
		t->columns[t->columns_count].group = group;
		t->columns_count++;
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

		for (i = t->xc; i < n; i++) nc[i] = WIDTH_AUTO;
		t->xc = n;
		t->xcols = nc;
	}

	if (t->xcols[x] == WIDTH_AUTO || f) {
		t->xcols[x] = width;
		return;
	}

	if (width == WIDTH_AUTO) return;

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
		if (!strlcasecmp(name, namelen, "TABLE", 5)) level++;
		if (!strlcasecmp(name, namelen, "/TABLE", 6)) {
			level--;
			if (!level) return html;
		}
	}
}

static struct table *
parse_table(unsigned char *html, unsigned char *eof,
	    unsigned char **end, color_t bgcolor,
	    int sh, struct html_start_end **bad_html, int *bhp)
{
	struct table *t;
	struct table_cell *cell;
	unsigned char *t_name, *t_attr, *en;
	unsigned char *lbhp = NULL;
	unsigned char *l_fragment_id = NULL;
	color_t l_col = bgcolor;
	int t_namelen;
	int p = 0;
	int l_al = AL_LEFT;
	int l_val = VALIGN_MIDDLE;
	int colspan, rowspan;
	int group = 0;
	int i, j, k;
	int qqq;
	int c_al = AL_TR, c_val = VALIGN_TR, c_width = WIDTH_AUTO, c_span = 0;
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
		if (!realloc_bad_html(bad_html, *bhp)) {
			goto qwe;
		}
		lbhp = html;
		(*bad_html)[(*bhp)++].start = html;
	}

qwe:
	while (html < eof && *html != '<') html++;

	if (html >= eof) {
		if (p) CELL(t, x, y)->end = html;
		if (lbhp) (*bad_html)[*bhp-1].end = html;
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

	if (!strlcasecmp(t_name, t_namelen, "TABLE", 5)) {
		en = skip_table(en, eof);
		goto see;
	}

	if (!strlcasecmp(t_name, t_namelen, "/TABLE", 6)) {
		if (c_span) new_columns(t, c_span, c_width, c_al, c_val, 1);
		if (p) CELL(t, x, y)->end = html;
		if (lbhp) (*bad_html)[*bhp-1].end = html;
		goto scan_done;
	}

	if (!strlcasecmp(t_name, t_namelen, "COLGROUP", 8)) {
		if (c_span) new_columns(t, c_span, c_width, c_al, c_val, 1);
		if (lbhp) {
			(*bad_html)[*bhp-1].end = html;
			lbhp = NULL;
		}
		c_al = AL_TR;
		c_val = VALIGN_TR;
		c_width = WIDTH_AUTO;
		get_align(t_attr, &c_al);
		get_valign(t_attr, &c_val);
		get_column_width(t_attr, &c_width, sh);
		c_span = get_num(t_attr, "span");
		if (c_span == -1) c_span = 1;
		goto see;
	}

	if (!strlcasecmp(t_name, t_namelen, "/COLGROUP", 9)) {
		if (c_span) new_columns(t, c_span, c_width, c_al, c_val, 1);
		if (lbhp) {
			(*bad_html)[*bhp-1].end = html;
			lbhp = NULL;
		}
		c_span = 0;
		c_al = AL_TR;
		c_val = VALIGN_TR;
		c_width = WIDTH_AUTO;
		goto see;
	}

	if (!strlcasecmp(t_name, t_namelen, "COL", 3)) {
		int sp, width, al, val;

		if (lbhp) {
			(*bad_html)[*bhp-1].end = html;
			lbhp = NULL;
		}

		sp = get_num(t_attr, "span");
		if (sp == -1) sp = 1;

		width = c_width;
		al = c_al;
		val = c_val;
		get_align(t_attr, &al);
		get_valign(t_attr, &val);
		get_column_width(t_attr, &width, sh);
		new_columns(t, sp, width, al, val, !!c_span);
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
				(*bad_html)[*bhp-1].end = html;
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
			(*bad_html)[*bhp-1].end = html;
			lbhp = NULL;
		}

		if (group) group--;
		l_al = AL_LEFT;
		l_val = VALIGN_MIDDLE;
		l_col = bgcolor;
		get_align(t_attr, &l_al);
		get_valign(t_attr, &l_val);
		get_bgcolor(t_attr, &l_col);
		if (l_fragment_id) mem_free(l_fragment_id);
		l_fragment_id = get_attr_val(t_attr, "id");
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
			(*bad_html)[*bhp-1].end = html;
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
		(*bad_html)[*bhp-1].end = html;
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

	if (cell->is_used) {
		if (cell->colspan == -1) goto see;
		x++;
		goto nc;
	}

	p = 1;

	cell->mx = x;
	cell->my = y;
	cell->is_used = 1;
	cell->start = en;

	cell->align = l_al;
	cell->valign = l_val;
	cell->fragment_id = get_attr_val(t_attr, "id");
	if (!cell->fragment_id && l_fragment_id) {
		cell->fragment_id = l_fragment_id;
		l_fragment_id = NULL;
	}

	cell->is_header = (upcase(t_name[1]) == 'H');
	if (cell->is_header) cell->align = AL_CENTER;

	if (group == 1) cell->group = 1;

	if (x < t->columns_count) {
		if (t->columns[x].align != AL_TR)
			cell->align = t->columns[x].align;
		if (t->columns[x].valign != VALIGN_TR)
			cell->valign = t->columns[x].valign;
	}

	cell->bgcolor = l_col;

	get_align(t_attr, &cell->align);
	get_valign(t_attr, &cell->valign);
	get_bgcolor(t_attr, &cell->bgcolor);

	colspan = get_num(t_attr, "colspan");
	if (colspan == -1) colspan = 1;
	else if (!colspan) colspan = -1;

	rowspan = get_num(t_attr, "rowspan");
	if (rowspan == -1) rowspan = 1;
	else if (!rowspan) rowspan = -1;

	cell->colspan = colspan;
	cell->rowspan = rowspan;

	if (colspan == 1) {
		int width = WIDTH_AUTO;

		get_column_width(t_attr, &width, sh);
		if (width != WIDTH_AUTO) set_td_width(t, x, width, 0);
	}

	qqq = t->x;

	for (i = 1; colspan != -1 ? i < colspan : i < qqq; i++) {
		struct table_cell *sc = new_cell(t, x + i, y);

		if (!sc || sc->is_used) {
			colspan = i;
			for (k = 0; k < i; k++) CELL(t, x + k, y)->colspan = colspan;
			break;
		}

		sc->is_used = sc->is_spanned = 1;
		sc->rowspan = rowspan;
		sc->colspan = colspan;
		sc->mx = x;
		sc->my = y;
	}

	qqq = t->y;
	for (j = 1; rowspan != -1 ? j < rowspan : j < qqq; j++) {
		for (k = 0; k < i; k++) {
			struct table_cell *sc = new_cell(t, x + k, y + j);

			if (!sc || sc->is_used) {
				int l, m;

				if (sc->mx == x && sc->my == y) continue;

				/* INTERNAL("boo"); */

				for (l = 0; l < k; l++)
					memset(CELL(t, x + l, y + j), 0,
					       sizeof(struct table_cell));

				rowspan = j;

				for (l = 0; l < i; l++)
					for (m = 0; m < j; m++)
						CELL(t, x + l, y + m)->rowspan = j;
				goto see;
			}

			sc->is_used = sc->is_spanned = 1;
			sc->rowspan = rowspan;
			sc->colspan = colspan;
			sc->mx = x;
			sc->my = y;
		}
	}

	goto see;

scan_done:
	*end = html;

	if (l_fragment_id) mem_free(l_fragment_id);

	for (x = 0; x < t->x; x++) for (y = 0; y < t->y; y++) {
		struct table_cell *c = CELL(t, x, y);

		if (!c->is_spanned) {
			if (c->colspan == -1) c->colspan = t->x - x;
			if (c->rowspan == -1) c->rowspan = t->y - y;
		}
	}

	if (t->y) {
		t->rows_height = mem_calloc(t->y, sizeof(int));
		if (!t->rows_height) {
			free_table(t);
			return NULL;
		}
	} else t->rows_height = NULL;

	for (x = 0; x < t->columns_count; x++)
		if (t->columns[x].width != WIDTH_AUTO)
			set_td_width(t, x, t->columns[x].width, 1);
	set_td_width(t, t->x, WIDTH_AUTO, 0);

	return t;
}

static inline struct part *
format_cell(struct table *table, int column, int row,
	    struct document *document, int x, int y, int width)
{
	struct table_cell *cell = CELL(table, column, row);

	if (document) {
		x += table->p->x;
		y += table->p->y;
	}

	return format_html_part(cell->start, cell->end, cell->align,
				table->cellpadding, width, document, x, y,
				NULL, cell->link_num);
}

static inline void
get_cell_width(unsigned char *start, unsigned char *end, int cellpadding, int w,
	       int a, int *min, int *max, int n_link, int *n_links)
{
	struct part *p;

	if (min) *min = -1;
	if (max) *max = -1;
	if (n_links) *n_links = n_link;

	p = format_html_part(start, end, AL_LEFT, cellpadding, w, NULL, !!a, !!a,
			     NULL, n_link);
	if (!p) return;

	if (min) *min = p->width;
	if (max) *max = p->max_width;
	if (n_links) *n_links = p->link_num;

	if (min && max) {
		assertm(*min <= *max, "get_cell_width: %d > %d", *min, *max);
	}

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

		get_cell_width(c->start, c->end, t->cellpadding, 0, 0,
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

	if (!global_doc_opts->table_order)
		for (j = 0; j < t->y; j++)
			for (i = 0; i < t->x; i++) {
				struct table_cell *c = CELL(t, i, j);

				if (!c->start) continue;
				c->link_num = nl;
				get_cell_width(c->start, c->end, t->cellpadding, 0, 0,
					       &c->min_width, &c->max_width, nl, &nl);
			}
	else
		for (i = 0; i < t->x; i++)
			for (j = 0; j < t->y; j++) {
				struct table_cell *c = CELL(t, i, j);

				if (!c->start) continue;
				c->link_num = nl;
				get_cell_width(c->start, c->end, t->cellpadding, 0, 0,
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

	if (t->rules == TABLE_RULE_COLS || t->rules == TABLE_RULE_ALL)
		w = t->cellspacing;
	else if (t->rules == TABLE_RULE_GROUPS)
		w = (col < t->columns_count && t->columns[col].group);

	if (!w && t->cellpadding) w = -1;

	return w;
}

static int
get_hline_width(struct table *t, int row)
{
	if (!row) return -1;

	if (t->rules == TABLE_RULE_ROWS || t->rules == TABLE_RULE_ALL) {
		if (t->cellspacing || t->vcellpadding)
			return t->cellspacing;
		return -1;

	} else if (t->rules == TABLE_RULE_GROUPS) {
		register int col;

		for (col = 0; col < t->x; col++)
			if (CELL(t, col, row)->group) {
				if (t->cellspacing || t->vcellpadding)
					return t->cellspacing;
				return -1;
			}
	}

	return t->vcellpadding ? 0 : -1;
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

	if (!t->columns_width) {
		t->columns_width = mem_calloc(t->x, sizeof(int));
		if (!t->columns_width) {
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

			if (c->is_spanned || !c->is_used) continue;

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
		if (t->frame & TABLE_FRAME_LHS) {
			min++;
			max++;
		}
		if (t->frame & TABLE_FRAME_RHS) {
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
	memcpy(t->columns_width, t->min_c, tx_size);
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
					if (t->columns_width[i] < t->xcols[i]) {
						w[i] = 1;
						mx[i] = int_min(t->xcols[i], t->max_c[i])
							- t->columns_width[i];
						if (mx[i] <= 0) w[i] = 0;
					}

					break;
				case 1:
					if (t->xcols[i] <= WIDTH_RELATIVE) {
						w[i] = WIDTH_RELATIVE - t->xcols[i];
						mx[i] = t->max_c[i] - t->columns_width[i];
						if (mx[i] <= 0) w[i] = 0;
					}
					break;
				case 2:
					if (t->xcols[i] != WIDTH_AUTO)
						break;
					/* Fall-through */
				case 3:
					if (t->columns_width[i] < t->max_c[i]) {
						mx[i] = t->max_c[i] - t->columns_width[i];
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
						mx[i] = t->xcols[i] - t->columns_width[i];
						if (mx[i] <= 0) w[i] = 0;
					}
					break;
				case 5:
					if (t->xcols[i] < 0) {
						if (t->xcols[i] <= WIDTH_RELATIVE) {
							w[i] = WIDTH_RELATIVE - t->xcols[i];
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
					INTERNAL("could not expand table");
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
			int q = t->columns_width[mii];

			if (u) u[mii] = 1;
			t->columns_width[mii] += mss;
			d -= t->columns_width[mii] - q;
			while (d < 0) {
				t->columns_width[mii]--;
				d++;
			}
			assertm(t->columns_width[mii] >= q, "shrinking cell");
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
			p += t->columns_width[i + k] +
			     (k && get_vline_width(t, i + k) >= 0);
		}

		get_cell_width(c->start, c->end, t->cellpadding, p, 1, &c->width,
			       NULL, c->link_num, NULL);

		int_upper_bound(&c->width, p);
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

				dst_width(w + i, s, c->width - p, t->max_c + i);

			} else if (c->colspan > s && c->colspan < ns) {
				ns = c->colspan;
			}
		}
		s = ns;
	} while (s != MAXINT);

	s = ns = 0;
	for (i = 0; i < t->x; i++) {
		s += t->columns_width[i];
		ns += w[i];
	}

	if (ns > s) {
		/* INTERNAL("new width(%d) is larger than previous(%d)", ns, s); */
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
			mem_free(t->columns_width);
			t->columns_width = w;
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

			if (!cell->is_used || cell->is_spanned) continue;

			for (sp = 0; sp < cell->colspan; sp++) {
				xw += t->columns_width[i + sp] +
				      (sp < cell->colspan - 1 &&
				       get_vline_width(t, i + sp + 1) >= 0);
			}

			p = format_cell(t, i, j, NULL, 2, 2, xw);
			if (!p) return;

			cell->height = p->height;
			/* DBG("%d, %d.",xw, cell->height); */
			mem_free(p);
		}
	}

	do {
		int ns = MAXINT;

		for (j = 0; j < t->y; j++) {
			for (i = 0; i < t->x; i++) {
				struct table_cell *cell = CELL(t, i, j);

				if (!cell->is_used || cell->is_spanned) continue;

				if (cell->rowspan == s) {
					register int k, p = 0;

					for (k = 1; k < s; k++)
						p += (get_hline_width(t, j + k) >= 0);

					dst_width(t->rows_height + j, s,
						  cell->height - p, NULL);

				} else if (cell->rowspan > s &&
					   cell->rowspan < ns) {
					ns = cell->rowspan;
				}

			}
		}
		s = ns;
	} while (s != MAXINT);

	{
		struct table_frames tf;

		get_table_frames(t, &tf);
		t->rh = tf.top + tf.bottom;
		for (j = 0; j < t->y; j++) {
			t->rh += t->rows_height[j] +
				 (j && get_hline_width(t, j) >= 0);
		}
	}
}

static void
display_complicated_table(struct table *t, int x, int y, int *yy)
{
	register int i, j;
	struct document *document = t->p->document;
	int xp, yp;
	int expand_cols = (global_doc_opts && global_doc_opts->table_expand_cols);
	color_t default_bgcolor = par_format.bgcolor;
	struct table_frames tf;

	get_table_frames(t, &tf);

	if (t->fragment_id)
		add_fragment_identifier(t->p, t->fragment_id);

	xp = x + tf.left;
	for (i = 0; i < t->x; i++) {
		yp = y + tf.top;

		for (j = 0; j < t->y; j++) {
			struct table_cell *cell = CELL(t, i, j);
			int rows_height = t->rows_height[j] +
				(j < t->y - 1 && get_hline_width(t, j + 1) >= 0);
			int row;

			par_format.bgcolor = default_bgcolor;
			for (row = t->p->cy; row < yp + rows_height + tf.top; row++) {
				expand_lines(t->p, row);
				expand_line(t->p, row, x - 1);
			}

			if (cell->start) {
				struct part *p = NULL;
				int xw = 0;
				int yw = 0;
				register int s;
				struct html_element *state;

				for (s = 0; s < cell->colspan; s++) {
					xw += t->columns_width[i + s] +
					      (s < cell->colspan - 1 &&
					       get_vline_width(t, i + s + 1) >= 0);
				}

				for (s = 0; s < cell->rowspan; s++) {
					yw += t->rows_height[j + s] +
					      (s < cell->rowspan - 1 &&
					       get_hline_width(t, j + s + 1) >= 0);
				}

				if (expand_cols) {
					/* This is not working correctly. Some
					 * pages will be rendered much better
					 * (/.) while other will look very ugly
					 * and broken. */
					par_format.bgcolor = t->bgcolor;
					for (s = yp; s < yp + yw; s++) {
						expand_lines(t->p, s);
						expand_line(t->p, s, xp - 1);
					}
				}

				state = init_html_parser_state(ELEMENT_DONT_KILL,
							       par_format.align, 0, 0);

				if (cell->is_header) format.attr |= AT_BOLD;

				format.bg = cell->bgcolor;
				par_format.bgcolor = cell->bgcolor;
 				{
					int tmpy = yp;

					if (cell->valign == VALIGN_MIDDLE)
						tmpy += (yw - cell->height)>>1;
					else if (cell->valign == VALIGN_BOTTOM)
						tmpy += (yw - cell->height);

				   	p = format_cell(t, i, j, document, xp, tmpy, xw);
				}

				if (p) {
					int yt;

					for (yt = 0; yt < p->height; yt++) {
						expand_lines(t->p, yp + yt);
						expand_line(t->p, yp + yt, xp + t->columns_width[i]);
					}

					if (cell->fragment_id)
						add_fragment_identifier(p, cell->fragment_id);

					mem_free(p);
				}

				done_html_parser_state(state);
			}

			yp += t->rows_height[j] +
			      (j < t->y - 1 && get_hline_width(t, j + 1) >= 0);
		}

		if (i < t->x - 1) {
			xp += t->columns_width[i] + (get_vline_width(t, j + 1) >= 0);
		}
	}

	yp = y;
	for (j = 0; j < t->y; j++) {
		yp += t->rows_height[j] +
		      (j < t->y - 1 && get_hline_width(t, j + 1) >= 0);
	}

	*yy = yp + tf.top + tf.bottom;
}


static inline int
get_frame_pos(int a, int a_size, int b, int b_size)
{
	assert(a >= -1 || a < a_size + 2 || b >= 0 || b <= b_size);
	if_assert_failed return 0;
	return a + 1 + (a_size + 2) * b;
}

#define H_FRAME_POSITION(tt, xx, yy) frame[0][get_frame_pos(xx, (tt)->x, yy, (tt)->y)]
#define V_FRAME_POSITION(tt, xx, yy) frame[1][get_frame_pos(yy, (tt)->y, xx, (tt)->x)]

static inline void
draw_frame_point(struct table *table, signed char *frame[2], int x, int y,
		 int i, int j, color_t bgcolor, color_t fgcolor)
{
	/* TODO: Use /BORDER._.* / macros ! --pasky */
	static unsigned char border_chars[81] = {
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
	/* Note: I have no clue wether any of these names are suitable but they
	 * should give an idea of what is going on. --jonas */
	signed char left   = H_FRAME_POSITION(table, i - 1,     j);
	signed char right  = H_FRAME_POSITION(table,     i,     j);
	signed char top    = V_FRAME_POSITION(table,     i, j - 1);
	signed char bottom = V_FRAME_POSITION(table,     i,     j);
	register int pos;

	if (left < 0 && right < 0 && top < 0 && bottom < 0) return;

	pos =      int_max(top,    0)
	    +  3 * int_max(right,  0)
	    +  9 * int_max(left,   0)
	    + 27 * int_max(bottom, 0);

	draw_frame_hchars(table->p, x, y, 1, border_chars[pos], bgcolor, fgcolor);
}

static inline void
draw_frame_hline(struct table *table, signed char *frame[2], int x, int y,
		 int i, int j, color_t bgcolor, color_t fgcolor)
{
 	static unsigned char hltable[] = { ' ', BORDER_SHLINE, BORDER_DHLINE };
 	int pos = H_FRAME_POSITION(table, i, j);

 	assertm(pos < 3, "Horizontal table position out of bound [%d]", pos);
	if_assert_failed return;

 	if (pos < 0 || table->columns_width[i] <= 0) return;

 	draw_frame_hchars(table->p, x, y, table->columns_width[i], hltable[pos], bgcolor, fgcolor);
}

static inline void
draw_frame_vline(struct table *table, signed char *frame[2], int x, int y,
		 int i, int j, color_t bgcolor, color_t fgcolor)
{
 	static unsigned char vltable[] = { ' ', BORDER_SVLINE, BORDER_DVLINE };
 	int pos = V_FRAME_POSITION(table, i, j);

 	assertm(pos < 3, "Vertical table position out of bound [%d]", pos);
	if_assert_failed return;

 	if (pos < 0 || table->rows_height[j] <= 0) return;

 	draw_frame_vchars(table->p, x, y, table->rows_height[j], vltable[pos], bgcolor, fgcolor);
}

static void
display_table_frames(struct table *t, int x, int y)
{
	struct table_frames tf;
 	signed char *frame[2];
  	register int i, j;
  	int cx, cy;
  	int fh_size = (t->x + 2) * (t->y + 1);
  	int fv_size = (t->x + 1) * (t->y + 2);

 	frame[0] = fmem_alloc(fh_size + fv_size);
 	if (!frame[0]) return;
 	memset(frame[0], -1, fh_size + fv_size);

 	frame[1] = &frame[0][fh_size];

	if (t->rules == TABLE_RULE_NONE) goto cont2;

	for (j = 0; j < t->y; j++) for (i = 0; i < t->x; i++) {
		int xsp, ysp;
		struct table_cell *cell = CELL(t, i, j);

		if (!cell->is_used || cell->is_spanned) continue;

		xsp = cell->colspan ? cell->colspan : t->x - i;
		ysp = cell->rowspan ? cell->rowspan : t->y - j;

		if (t->rules != TABLE_RULE_COLS) {
			memset(&H_FRAME_POSITION(t, i, j), t->cellspacing, xsp);
			memset(&H_FRAME_POSITION(t, i, j + ysp), t->cellspacing, xsp);
		}

		if (t->rules != TABLE_RULE_ROWS) {
			memset(&V_FRAME_POSITION(t, i, j), t->cellspacing, ysp);
			memset(&V_FRAME_POSITION(t, i + xsp, j), t->cellspacing, ysp);
		}
	}

	if (t->rules == TABLE_RULE_GROUPS) {
		for (i = 1; i < t->x; i++) {
			if (!t->xcols[i])
				memset(&V_FRAME_POSITION(t, i, 0), 0, t->y);
		}

		for (j = 1; j < t->y; j++) {
			for (i = 0; i < t->x; i++)
				if (CELL(t, i, j)->group)
					goto cont;

			memset(&H_FRAME_POSITION(t, 0, j), 0, t->x);
cont:;
		}
	}

cont2:

	get_table_frames(t, &tf);
	memset(&H_FRAME_POSITION(t, 0, 0), tf.top, t->x);
	memset(&H_FRAME_POSITION(t, 0, t->y), tf.bottom, t->x);
	memset(&V_FRAME_POSITION(t, 0, 0), tf.left, t->y);
	memset(&V_FRAME_POSITION(t, t->x, 0), tf.right, t->y);

	cy = y;
	for (j = 0; j <= t->y; j++) {
		cx = x;
		if ((j > 0 && j < t->y && get_hline_width(t, j) >= 0)
		    || (j == 0 && tf.top)
		    || (j == t->y && tf.bottom)) {
			int w = tf.left ? t->border : -1;

			for (i = 0; i < t->x; i++) {
				if (i > 0)
					w = get_vline_width(t, i);

				if (w >= 0) {
					draw_frame_point(t, frame, cx, cy, i, j, par_format.bgcolor, t->bordercolor);
					if (j < t->y)
						draw_frame_vline(t, frame, cx, cy + 1, i, j, par_format.bgcolor, t->bordercolor);
					cx++;
				}

				draw_frame_hline(t, frame, cx, cy, i, j, par_format.bgcolor, t->bordercolor);
				cx += t->columns_width[i];
			}

			if (tf.right) {
				draw_frame_point(t, frame, cx, cy, i, j, par_format.bgcolor, t->bordercolor);
				if (j < t->y)
					draw_frame_vline(t, frame, cx, cy + 1, i, j, par_format.bgcolor, t->bordercolor);
				cx++;
			}

			cy++;

		} else if (j < t->y) {
			for (i = 0; i <= t->x; i++) {
				if ((i > 0 && i < t->x && get_vline_width(t, i) >= 0)
				    || (i == 0 && tf.left)
				    || (i == t->x && tf.right)) {
					draw_frame_vline(t, frame, cx, cy, i, j, par_format.bgcolor, t->bordercolor);
					cx++;
				}
				if (i < t->x) cx += t->columns_width[i];
			}
		}

		if (j < t->y) cy += t->rows_height[j];
		/*for (cyy = cy1; cyy < cy; cyy++) expand_line(t->p, cyy, cx - 1);*/
	}

	fmem_free(frame[0]);
}

static inline int
get_bordercolor(unsigned char *a, color_t *rgb)
{
	unsigned char *at;
	int r;

	if (!use_document_fg_colors(global_doc_opts))
		return -1;

	at = get_attr_val(a, "bordercolor");
	/* Try some other MSIE-specific attributes if any. */
	if (!at) at = get_attr_val(a, "bordercolorlight");
	if (!at) at = get_attr_val(a, "bordercolordark");
	if (!at) return -1;

	r = decode_color(at, strlen(at), rgb);
	mem_free(at);

	return r;
}

void
format_table(unsigned char *attr, unsigned char *html, unsigned char *eof,
	     unsigned char **end, void *f)
{
	struct part *p = f;
	struct table *t;
	struct html_start_end *bad_html;
	struct node *node, *new_node;
	unsigned char *al;
	struct html_element *state;
	color_t bgcolor = par_format.bgcolor;
	color_t bordercolor = 0;
	unsigned char *fragment_id;
	int border, cellspacing, vcellpadding, cellpadding, align;
	int frame, rules, width, wf;
	int cye;
	int x;
	int i;
	int bad_html_n;
	int cpd_pass, cpd_width, cpd_last;
	int margins;

	table_level++;
	get_bgcolor(attr, &bgcolor);
	get_bordercolor(attr, &bordercolor);

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

		cellspacing = get_num(attr, "cellspacing");
		int_bounds(&cellspacing, 1, 2);

		frame = TABLE_FRAME_BOX;
		al = get_attr_val(attr, "frame");
		if (al) {
			if (!strcasecmp(al, "void")) frame = TABLE_FRAME_VOID;
			else if (!strcasecmp(al, "above")) frame = TABLE_FRAME_ABOVE;
			else if (!strcasecmp(al, "below")) frame = TABLE_FRAME_BELOW;
			else if (!strcasecmp(al, "hsides")) frame = TABLE_FRAME_HSIDES;
			else if (!strcasecmp(al, "vsides")) frame = TABLE_FRAME_VSIDES;
			else if (!strcasecmp(al, "lhs")) frame = TABLE_FRAME_LHS;
			else if (!strcasecmp(al, "rhs")) frame = TABLE_FRAME_RHS;
			else if (!strcasecmp(al, "box")) frame = TABLE_FRAME_BOX;
			else if (!strcasecmp(al, "border")) frame = TABLE_FRAME_BOX;
			mem_free(al);
		}
	} else {
		cellspacing = 0;
		frame = TABLE_FRAME_VOID;
	}

	cellpadding = get_num(attr, "cellpadding");
	if (cellpadding == -1) {
		vcellpadding = 0;
		cellpadding = !!border;
	} else {
		vcellpadding = (cellpadding >= HTML_CHAR_HEIGHT / 2 + 1);
		cellpadding = (cellpadding >= HTML_CHAR_WIDTH / 2 + 1);
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

	rules = border ? TABLE_RULE_ALL : TABLE_RULE_NONE;
	al = get_attr_val(attr, "rules");
	if (al) {
		if (!strcasecmp(al, "none")) rules = TABLE_RULE_NONE;
		else if (!strcasecmp(al, "groups")) rules = TABLE_RULE_GROUPS;
		else if (!strcasecmp(al, "rows")) rules = TABLE_RULE_ROWS;
		else if (!strcasecmp(al, "cols")) rules = TABLE_RULE_COLS;
		else if (!strcasecmp(al, "all")) rules = TABLE_RULE_ALL;
		mem_free(al);
	}

	fragment_id = get_attr_val(attr, "id");

	wf = 0;
	width = get_width(attr, "width", (p->document || p->x));
	if (width == -1) {
		width = par_format.width - par_format.leftmargin - par_format.rightmargin;
		if (width < 0) width = 0;
		wf = 1;
	}

	t = parse_table(html, eof, end, bgcolor, (p->document || p->x), &bad_html, &bad_html_n);
	if (!t) {
		if (bad_html) mem_free(bad_html);
		goto ret0;
	}

	for (i = 0; i < bad_html_n; i++) {
		while (bad_html[i].start < bad_html[i].end && isspace(*bad_html[i].start))
			bad_html[i].start++;
		while (bad_html[i].start < bad_html[i].end && isspace(bad_html[i].end[-1]))
			bad_html[i].end--;
		if (bad_html[i].start < bad_html[i].end)
			parse_html(bad_html[i].start, bad_html[i].end, p, NULL);
	}

	if (bad_html) mem_free(bad_html);

	state = init_html_parser_state(ELEMENT_DONT_KILL, AL_LEFT, 0, 0);

	t->p = p;
	t->border = border;
	t->bordercolor = bordercolor;
	t->cellpadding = cellpadding;
	t->vcellpadding = vcellpadding;
	t->cellspacing = cellspacing;
	t->frame = frame;
	t->rules = rules;
	t->width = width;
	t->wf = wf;
	t->fragment_id = fragment_id;
	fragment_id = NULL;

	cpd_pass = 0;
	cpd_last = t->cellpadding;
	cpd_width = 0;  /* not needed, but let the warning go away */

again:
	get_cell_widths(t);
	if (get_column_widths(t)) goto ret2;

	get_table_width(t);

	margins = par_format.leftmargin + par_format.rightmargin;
	if (!p->document && !p->x) {
		if (!wf) int_upper_bound(&t->max_t, width);
		int_lower_bound(&t->max_t, t->min_t);

		p->max_width = int_max(p->max_width, t->max_t + margins);
		p->width = int_max(p->width, t->min_t + margins);

		goto ret2;
	}

	if (!cpd_pass && t->min_t > width && t->cellpadding) {
		t->cellpadding = 0;
		cpd_pass = 1;
		cpd_width = t->min_t;
		goto again;
	}
	if (cpd_pass == 1 && t->min_t > cpd_width) {
		t->cellpadding = cpd_last;
		cpd_pass = 2;
		goto again;
	}

	/* DBG("%d %d %d", t->min_t, t->max_t, width); */
	if (t->min_t >= width)
		distribute_widths(t, t->min_t);
	else if (t->max_t < width && wf)
		distribute_widths(t, t->max_t);
	else
		distribute_widths(t, width);

	if (!p->document && p->x == 1) {
		int ww = t->rw + margins;

		int_bounds(&ww, t->rw, par_format.width);
		p->width = int_max(p->width, ww);
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
		p->width = int_max(p->width, t->rw + margins);
		p->cy += t->rh;
		goto ret2;
	}

	node = p->document->nodes.next;
	node->height = p->y - node->y + p->cy;

	display_complicated_table(t, x, p->cy, &cye);
	display_table_frames(t, x, p->cy);

	new_node = mem_alloc(sizeof(struct node));
	if (new_node) {
		new_node->x = node->x;
		new_node->y = p->y + cye;
		new_node->width = node->width;
		add_to_list(p->document->nodes, new_node);
	}

	assertm(p->cy + t->rh == cye, "size does not match; 1:%d, 2:%d",
		p->cy + t->rh, cye);

	p->cy = cye;
	p->cx = -1;

ret2:
	p->link_num = t->link_num;
	p->height = int_max(p->height, p->cy);
	free_table(t);
	done_html_parser_state(state);

ret0:
	table_level--;
	if (fragment_id) mem_free(fragment_id);
	if (!table_level) free_table_cache();
}
