/* HTML tables renderer */
/* $Id: tables.c,v 1.203 2004/06/25 10:06:08 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/html/parser/parse.h"
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
#define TABLE_FRAME_HSIDES	(TABLE_FRAME_ABOVE | TABLE_FRAME_BELOW)
#define TABLE_FRAME_LHS		4
#define TABLE_FRAME_RHS		8
#define TABLE_FRAME_VSIDES	(TABLE_FRAME_LHS | TABLE_FRAME_RHS)
#define TABLE_FRAME_BOX		(TABLE_FRAME_HSIDES | TABLE_FRAME_VSIDES)

#define TABLE_RULE_NONE		0
#define TABLE_RULE_ROWS		1
#define TABLE_RULE_COLS		2
#define TABLE_RULE_ALL		3
#define TABLE_RULE_GROUPS	4

#define INIT_X		2
#define INIT_Y		2

#define realloc_bad_html(bad_html, size) \
	mem_align_alloc(bad_html, size, (size) + 1, struct html_start_end, 0xFF)

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
	struct part *part;
	struct table_cell *cells;
	unsigned char *fragment_id;
	color_t bgcolor;
	color_t bordercolor;

	struct table_column *columns;
	int columns_count;
	int real_columns_count;	/* Number of columns really allocated. */

	int *min_cols_widths;
        int *max_cols_widths;
	int *cols_widths;
	int *cols_x;
	int cols_x_count;

	int *rows_heights;

	int x, y;
	int rx, ry;
	int border;
	int cellpadding;
	int vcellpadding;
	int cellspacing;
	int frame, rules, width;
	/* int has_width; not used. */
	int real_width;
	int real_height;
	int min_width;
	int max_width;

	int link_num;
};

struct html_start_end {
	unsigned char *start, *end;
};

struct table_frames {
	unsigned int top:1;
	unsigned int bottom:1;
	unsigned int left:1;
	unsigned int right:1;
};

static void
get_table_frames(struct table *table, struct table_frames *result)
{
	assert(table && result);

	if (table->border) {
		result->top = !!(table->frame & TABLE_FRAME_ABOVE);
		result->bottom = !!(table->frame & TABLE_FRAME_BELOW);
		result->left = !!(table->frame & TABLE_FRAME_LHS);
		result->right = !!(table->frame & TABLE_FRAME_RHS);
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
	int len;

	if (!al) return;

	len = strlen(al);
	if (len && al[len - 1] == '*') {
		unsigned char *en;
		int n;

		al[len - 1] = '\0';
		errno = 0;
		n = strtoul(al, (char **) &en, 10);
		if (!errno && n >= 0 && !*en)
			*width = WIDTH_RELATIVE - n;
	} else {
		int w = get_width(attr, "width", sh);

		if (w >= 0) *width = w;
	}
	mem_free(al);
}

static struct table *
new_table(void)
{
	struct table *table = mem_calloc(1, sizeof(struct table));

	if (!table) return NULL;

	table->cells = mem_calloc(INIT_X * INIT_Y, sizeof(struct table_cell));
	if (!table->cells) {
		mem_free(table);
		return NULL;
	}

	table->columns = mem_calloc(INIT_X, sizeof(struct table_column));
	if (!table->columns) {
		mem_free(table->cells);
		mem_free(table);
		return NULL;
	}

	table->rx = INIT_X;
	table->ry = INIT_Y;
	table->real_columns_count = INIT_X;

	return table;
}

static void
free_table(struct table *table)
{
	int i, j;

	mem_free_if(table->min_cols_widths);
	mem_free_if(table->max_cols_widths);
	mem_free_if(table->cols_widths);
	mem_free_if(table->rows_heights);
	mem_free_if(table->fragment_id);
	mem_free_if(table->cols_x);

	for (i = 0; i < table->x; i++)
		for (j = 0; j < table->y; j++)
			mem_free_if(CELL(table, i, j)->fragment_id);

	mem_free(table->cells);
	mem_free(table->columns);
	mem_free(table);
}

static void
expand_cells(struct table *table, int x, int y)
{
	if (x >= table->x) {
		if (table->x) {
			int tx = table->x - 1;
			register int i;

			for (i = 0; i < table->y; i++) {
				register int j;
				struct table_cell *cellp = CELL(table, tx, i);

				if (cellp->colspan != -1) continue;

				for (j = table->x; j <= x; j++) {
					struct table_cell *cell = CELL(table, j, i);

					cell->is_used = 1;
					cell->is_spanned = 1;
					cell->rowspan = cellp->rowspan;
					cell->colspan = -1;
					cell->mx = cellp->mx;
					cell->my = cellp->my;
				}
			}
		}
		table->x = x + 1;
	}

	if (y >= table->y) {
		if (table->y) {
			int ty = table->y - 1;
			register int i;

			for (i = 0; i < table->x; i++) {
				register int j;
				struct table_cell *cellp = CELL(table, i, ty);

				if (cellp->rowspan != -1) continue;

				for (j = table->y; j <= y; j++) {
					struct table_cell *cell = CELL(table, i, j);

					cell->is_used = 1;
					cell->is_spanned = 1;
					cell->rowspan = -1;
					cell->colspan = cellp->colspan;
					cell->mx = cellp->mx;
					cell->my = cellp->my;
				}
			}
		}
		table->y = y + 1;
	}
}

static struct table_cell *
new_cell(struct table *table, int x, int y)
{
	if (x < table->x && y < table->y)
		return CELL(table, x, y);

	while (1) {
		struct table new_table;
		register int i = 0;

		if (x < table->rx && y < table->ry) {
			expand_cells(table, x, y);
			return CELL(table, x, y);
		}

		new_table.rx = table->rx;
		new_table.ry = table->ry;

		while (x >= new_table.rx)
			if (!(new_table.rx <<= 1))
				return NULL;
		while (y >= new_table.ry)
			if (!(new_table.ry <<= 1))
				return NULL;

		new_table.cells = mem_calloc(new_table.rx * new_table.ry,
					     sizeof(struct table_cell));
		if (!new_table.cells) return NULL;

		while (i < table->x) {
			register int j = 0;

			while (j < table->y) {
				memcpy(CELL(&new_table, i, j), CELL(table, i, j),
				       sizeof(struct table_cell));
				j++;
			}
			i++;
		}

		mem_free(table->cells);
		table->cells = new_table.cells;
		table->rx = new_table.rx;
		table->ry = new_table.ry;
	}
}

static void
new_columns(struct table *table, int span, int width, int align,
	    int valign, int group)
{
	if (table->columns_count + span > table->real_columns_count) {
		int n = table->real_columns_count;
		struct table_column *new_columns;

		while (table->columns_count + span > n)
			if (!(n <<= 1))
				return;

		new_columns = mem_realloc(table->columns, n * sizeof(struct table_column));
		if (!new_columns) return;

		table->real_columns_count = n;
		table->columns = new_columns;
	}

	while (span--) {
		struct table_column *column = &table->columns[table->columns_count++];

		column->align = align;
		column->valign = valign;
		column->width = width;
		column->group = group;
		group = 0;
	}
}

static void
set_td_width(struct table *table, int col, int width, int force)
{
	if (col >= table->cols_x_count) {
		int n = table->cols_x_count;
		register int i;
		int *new_cols_x;

		while (col >= n) if (!(n <<= 1)) break;
		if (!n && table->cols_x_count) return;
		if (!n) n = col + 1;

		new_cols_x = mem_realloc(table->cols_x, n * sizeof(int));
		if (!new_cols_x) return;

		for (i = table->cols_x_count; i < n; i++)
			new_cols_x[i] = WIDTH_AUTO;
		table->cols_x_count = n;
		table->cols_x = new_cols_x;
	}

	if (force || table->cols_x[col] == WIDTH_AUTO) {
		table->cols_x[col] = width;
		return;
	}

	if (width == WIDTH_AUTO) return;

	if (width < 0 && table->cols_x[col] >= 0) {
		table->cols_x[col] = width;
		return;
	}

	if (width >= 0 && table->cols_x[col] < 0) return;
	table->cols_x[col] = (table->cols_x[col] + width) >> 1;
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
	struct table *table;
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
	table = new_table();
	if (!table) return NULL;

	table->bgcolor = bgcolor;
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
		if (p) CELL(table, x, y)->end = html;
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
		if (c_span) new_columns(table, c_span, c_width, c_al, c_val, 1);
		if (p) CELL(table, x, y)->end = html;
		if (lbhp) (*bad_html)[*bhp-1].end = html;
		goto scan_done;
	}

	if (!strlcasecmp(t_name, t_namelen, "COLGROUP", 8)) {
		if (c_span) new_columns(table, c_span, c_width, c_al, c_val, 1);
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
		if (c_span) new_columns(table, c_span, c_width, c_al, c_val, 1);
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
		new_columns(table, sp, width, al, val, !!c_span);
		c_span = 0;
		goto see;
	}

	/* /TR /TD /TH */
	if (t_namelen == 3
	    && t_name[0] == '/'
	    && toupper(t_name[1]) == 'T') {
	        unsigned char c = toupper(t_name[2]);

		if (c == 'R' || c == 'D' || c == 'H') {
	 		if (c_span)
				new_columns(table, c_span, c_width, c_al, c_val, 1);

			if (p) {
				CELL(table, x, y)->end = html;
				p = 0;
			}
			if (lbhp) {
				(*bad_html)[*bhp-1].end = html;
				lbhp = NULL;
			}
		}
	}

	/* All following tags have T as first letter. */
	if (toupper(t_name[0]) != 'T') goto see;

	/* TR */
	if (t_namelen == 2 && toupper(t_name[1]) == 'R') {
		if (c_span) new_columns(table, c_span, c_width, c_al, c_val, 1);

		if (p) {
			CELL(table, x, y)->end = html;
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
		mem_free_set(&l_fragment_id, get_attr_val(t_attr, "id"));
		y++;
		x = 0;
		goto see;
	}

	/* THEAD TBODY TFOOT */
	if (t_namelen == 5
	    && ((!strncasecmp(&t_name[1], "HEAD", 4)) ||
		(!strncasecmp(&t_name[1], "BODY", 4)) ||
		(!strncasecmp(&t_name[1], "FOOT", 4)))) {
		if (c_span) new_columns(table, c_span, c_width, c_al, c_val, 1);

		if (lbhp) {
			(*bad_html)[*bhp-1].end = html;
			lbhp = NULL;
		}

		group = 2;
	}

	/* TD TH */
	if (t_namelen != 2
	    || (toupper(t_name[1]) != 'D'
		&& toupper(t_name[1]) != 'H'))
		goto see;

	if (c_span) new_columns(table, c_span, c_width, c_al, c_val, 1);

	if (lbhp) {
		(*bad_html)[*bhp-1].end = html;
		lbhp = NULL;
	}
	if (p) {
		CELL(table, x, y)->end = html;
		p = 0;
	}

	if (y == -1) {
		y = 0;
		x = 0;
	}

	for (;;x++) {
		cell = new_cell(table, x, y);
		if (!cell) goto see;

		if (!cell->is_used) break;
		if (cell->colspan == -1) goto see;
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

	cell->is_header = (toupper(t_name[1]) == 'H');
	if (cell->is_header) cell->align = AL_CENTER;

	if (group == 1) cell->group = 1;

	if (x < table->columns_count) {
		if (table->columns[x].align != AL_TR)
			cell->align = table->columns[x].align;
		if (table->columns[x].valign != VALIGN_TR)
			cell->valign = table->columns[x].valign;
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
		if (width != WIDTH_AUTO)
			set_td_width(table, x, width, 0);
	}

	qqq = table->x;

	for (i = 1; colspan != -1 ? i < colspan : i < qqq; i++) {
		struct table_cell *span_cell = new_cell(table, x + i, y);

		if (!span_cell || span_cell->is_used) {
			colspan = i;
			for (k = 0; k < i; k++)
				CELL(table, x + k, y)->colspan = colspan;
			break;
		}

		span_cell->is_used = span_cell->is_spanned = 1;
		span_cell->rowspan = rowspan;
		span_cell->colspan = colspan;
		span_cell->mx = x;
		span_cell->my = y;
	}

	qqq = table->y;
	for (j = 1; rowspan != -1 ? j < rowspan : j < qqq; j++) {
		for (k = 0; k < i; k++) {
			struct table_cell *span_cell = new_cell(table, x + k, y + j);

			if (!span_cell || span_cell->is_used) {
				int l, m;

				if (span_cell->mx == x && span_cell->my == y)
					continue;

				for (l = 0; l < k; l++)
					memset(CELL(table, x + l, y + j), 0,
					       sizeof(struct table_cell));

				rowspan = j;

				for (l = 0; l < i; l++)
					for (m = 0; m < j; m++)
						CELL(table, x + l, y + m)->rowspan = j;
				goto see;
			}

			span_cell->is_used = span_cell->is_spanned = 1;
			span_cell->rowspan = rowspan;
			span_cell->colspan = colspan;
			span_cell->mx = x;
			span_cell->my = y;
		}
	}

	goto see;

scan_done:
	*end = html;

	mem_free_if(l_fragment_id);

	for (x = 0; x < table->x; x++) for (y = 0; y < table->y; y++) {
		struct table_cell *cell = CELL(table, x, y);

		if (!cell->is_spanned) {
			if (cell->colspan == -1) cell->colspan = table->x - x;
			if (cell->rowspan == -1) cell->rowspan = table->y - y;
		}
	}

	if (table->y) {
		table->rows_heights = mem_calloc(table->y, sizeof(int));
		if (!table->rows_heights) {
			free_table(table);
			return NULL;
		}
	} else table->rows_heights = NULL;

	for (x = 0; x < table->columns_count; x++)
		if (table->columns[x].width != WIDTH_AUTO)
			set_td_width(table, x, table->columns[x].width, 1);
	set_td_width(table, table->x, WIDTH_AUTO, 0);

	return table;
}

static inline struct part *
format_cell(struct table *table, int column, int row,
	    struct document *document, int x, int y, int width)
{
	struct table_cell *cell = CELL(table, column, row);

	if (document) {
		x += table->part->box.x;
		y += table->part->box.y;
	}

	return format_html_part(cell->start, cell->end, cell->align,
	                        table->cellpadding, width, document, x, y,
	                        NULL, cell->link_num);
}

static inline void
get_cell_width(unsigned char *start, unsigned char *end,
	       int cellpadding, int width,
	       int a, int *min, int *max, int n_link, int *n_links)
{
	struct part *part;

	if (min) *min = -1;
	if (max) *max = -1;
	if (n_links) *n_links = n_link;

	part = format_html_part(start, end, AL_LEFT, cellpadding, width, NULL,
			        !!a, !!a, NULL, n_link);
	if (!part) return;

	if (min) *min = part->box.width;
	if (max) *max = part->max_width;
	if (n_links) *n_links = part->link_num;

	if (min && max) {
		assertm(*min <= *max, "get_cell_width: %d > %d", *min, *max);
	}

	mem_free(part);
}

static inline void
check_cell_widths(struct table *table)
{
	register int i, j;

	for (j = 0; j < table->y; j++) for (i = 0; i < table->x; i++) {
		int min, max;
		struct table_cell *cell = CELL(table, i, j);

		if (!cell->start) continue;

		get_cell_width(cell->start, cell->end, table->cellpadding, 0, 0,
			       &min, &max, cell->link_num, NULL);

		assertm(!(min != cell->min_width || max < cell->max_width),
			"check_cell_widths failed");
	}
}

static inline void
get_cell_widths(struct table *table)
{
	int nl = table->part->link_num;
	register int i, j;

	if (!global_doc_opts->table_order)
		for (j = 0; j < table->y; j++)
			for (i = 0; i < table->x; i++) {
				struct table_cell *cell = CELL(table, i, j);

				if (!cell->start) continue;
				cell->link_num = nl;
				get_cell_width(cell->start, cell->end, table->cellpadding, 0, 0,
					       &cell->min_width, &cell->max_width, nl, &nl);
			}
	else
		for (i = 0; i < table->x; i++)
			for (j = 0; j < table->y; j++) {
				struct table_cell *cell = CELL(table, i, j);

				if (!cell->start) continue;
				cell->link_num = nl;
				get_cell_width(cell->start, cell->end, table->cellpadding, 0, 0,
					       &cell->min_width, &cell->max_width, nl, &nl);
			}

	table->link_num = nl;
}

static inline void
dst_width(int *values, int n, int w, int *limits)
{
	register int i;
	int s = 0, d, r, t;

	for (i = 0; i < n; i++) s += values[i];
	if (s >= w) return;

again:
	t = w - s;
	d = t / n;
	r = t % n;
	w = 0;

	if (limits) {
		for (i = 0; i < n; i++) {
			int delta;

			values[i] += d + (i < r);

			delta = values[i] - limits[i];
			if (delta > 0) {
				w += delta;
				values[i] = limits[i];
			}
		}
	} else {
		for (i = 0; i < n; i++) {
			values[i] += d + (i < r);
		}
	}

	if (w) {
		assertm(limits, "bug in dst_width");
		limits = NULL;
		s = 0;
		goto again;
	}
}


/* Returns: -1 none, 0, space, 1 line, 2 double */
static inline int
get_vline_width(struct table *table, int col)
{
	int w = 0;

	if (!col) return -1;

	if (table->rules == TABLE_RULE_COLS || table->rules == TABLE_RULE_ALL)
		w = table->cellspacing;
	else if (table->rules == TABLE_RULE_GROUPS)
		w = (col < table->columns_count && table->columns[col].group);

	if (!w && table->cellpadding) w = -1;

	return w;
}

static int
get_hline_width(struct table *table, int row)
{
	if (!row) return -1;

	if (table->rules == TABLE_RULE_ROWS || table->rules == TABLE_RULE_ALL) {
		if (table->cellspacing || table->vcellpadding)
			return table->cellspacing;
		return -1;

	} else if (table->rules == TABLE_RULE_GROUPS) {
		register int col;

		for (col = 0; col < table->x; col++)
			if (CELL(table, col, row)->group) {
				if (table->cellspacing || table->vcellpadding)
					return table->cellspacing;
				return -1;
			}
	}

	return table->vcellpadding ? 0 : -1;
}

static int
get_column_widths(struct table *table)
{
	int colspan;

	if (!table->x) return -1; /* prevents calloc(0, sizeof(int)) calls */

	if (!table->min_cols_widths) {
		table->min_cols_widths = mem_calloc(table->x, sizeof(int));
		if (!table->min_cols_widths) return -1;
	}

	if (!table->max_cols_widths) {
		table->max_cols_widths = mem_calloc(table->x, sizeof(int));
	   	if (!table->max_cols_widths) {
			mem_free_set(&table->min_cols_widths, NULL);
			return -1;
		}
	}

	if (!table->cols_widths) {
		table->cols_widths = mem_calloc(table->x, sizeof(int));
		if (!table->cols_widths) {
			mem_free_set(&table->min_cols_widths, NULL);
			mem_free_set(&table->max_cols_widths, NULL);
			return -1;
		}
	}

	colspan = 1;
	do {
		register int i = 0, j;
		int new_colspan = MAXINT;

		for (; i < table->x; i++) for (j = 0; j < table->y; j++) {
			struct table_cell *cell = CELL(table, i, j);

			if (cell->is_spanned || !cell->is_used) continue;

			assertm(cell->colspan + i <= table->x, "colspan out of table");
			if_assert_failed return -1;

			if (cell->colspan == colspan) {
				register int k, p = 0;

				for (k = 1; k < colspan; k++)
					p += (get_vline_width(table, i + k) >= 0);

				dst_width(&table->min_cols_widths[i], colspan,
				  	  cell->min_width - p,
					  &table->max_cols_widths[i]);

				dst_width(&table->max_cols_widths[i], colspan,
				  	  cell->max_width - p,
					  NULL);

				for (k = 0; k < colspan; k++) {
					int tmp = i + k;

					int_lower_bound(&table->max_cols_widths[tmp],
							table->min_cols_widths[tmp]);
				}

			} else if (cell->colspan > colspan
				   && cell->colspan < new_colspan) {
				new_colspan = cell->colspan;
			}
		}
		colspan = new_colspan;
	} while (colspan != MAXINT);

	return 0;
}

static void
get_table_width(struct table *table)
{
	struct table_frames table_frames;
	int min = 0;
	int max = 0;
	register int i;

	for (i = 0; i < table->x; i++) {
		int vl = (get_vline_width(table, i) >= 0);

		min += vl + table->min_cols_widths[i];
		max += vl + table->max_cols_widths[i];
		if (table->cols_x[i] > table->max_cols_widths[i])
			max += table->cols_x[i];
	}

	get_table_frames(table, &table_frames);

	table->min_width = min + table_frames.left + table_frames.right;
	table->max_width = max + table_frames.left + table_frames.right;

	assertm(min <= max, "min(%d) > max(%d)", min, max);
	/* XXX: Recovery path? --pasky */
}


/* TODO: understand and rewrite this thing... --Zas */
static void
distribute_widths(struct table *table, int width)
{
	register int i;
	int d = width - table->min_width;
	int om = 0;
	char *u;
	int *w, *mx;
	int mmax_c = 0;
	int tx_size;

	if (!table->x) return;

	assertm(d >= 0, "too small width %d, required %d", width, table->min_width);

	for (i = 0; i < table->x; i++)
		int_lower_bound(&mmax_c, table->max_cols_widths[i]);

	tx_size = table->x * sizeof(int);
	memcpy(table->cols_widths, table->min_cols_widths, tx_size);
	table->real_width = width;

	/* XXX: We don't need to fail if unsuccessful. See below. --Zas */
	u = fmem_alloc(table->x);

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

		for (i = 0; i < table->x; i++) {
			switch (om) {
				case 0:
					if (table->cols_widths[i] < table->cols_x[i]) {
						w[i] = 1;
						mx[i] = int_min(table->cols_x[i],
								table->max_cols_widths[i])
							- table->cols_widths[i];
						if (mx[i] <= 0) w[i] = 0;
					}

					break;
				case 1:
					if (table->cols_x[i] <= WIDTH_RELATIVE) {
						w[i] = WIDTH_RELATIVE - table->cols_x[i];
						mx[i] = table->max_cols_widths[i]
							- table->cols_widths[i];
						if (mx[i] <= 0) w[i] = 0;
					}
					break;
				case 2:
					if (table->cols_x[i] != WIDTH_AUTO)
						break;
					/* Fall-through */
				case 3:
					if (table->cols_widths[i] < table->max_cols_widths[i]) {
						mx[i] = table->max_cols_widths[i]
							- table->cols_widths[i];
						if (mmax_c) {
							w[i] = 5 + table->max_cols_widths[i] * 10 / mmax_c;
						} else {
							w[i] = 1;
						}
					}
					break;
				case 4:
					if (table->cols_x[i] >= 0) {
						w[i] = 1;
						mx[i] = table->cols_x[i] - table->cols_widths[i];
						if (mx[i] <= 0) w[i] = 0;
					}
					break;
				case 5:
					if (table->cols_x[i] < 0) {
						if (table->cols_x[i] <= WIDTH_RELATIVE) {
							w[i] = WIDTH_RELATIVE - table->cols_x[i];
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
		if (u) memset(u, 0, table->x);
		dd = d;

a:
		mss = 0;
		mii = -1;
		for (i = 0; i < table->x; i++) if (w[i]) {
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
			int q = table->cols_widths[mii];

			if (u) u[mii] = 1;
			table->cols_widths[mii] += mss;
			d -= table->cols_widths[mii] - q;
			while (d < 0) {
				table->cols_widths[mii]--;
				d++;
			}
			assertm(table->cols_widths[mii] >= q, "shrinking cell");
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
check_table_widths(struct table *table)
{
	register int i, j;
	int colspan;
	int width, new_width;
	int max, max_index = 0; /* go away, warning! */
	int *widths = mem_calloc(table->x, sizeof(int));

	if (!widths) return;

	for (j = 0; j < table->y; j++) for (i = 0; i < table->x; i++) {
		struct table_cell *cell = CELL(table, i, j);
		register int k, p = 0;

		if (!cell->start) continue;

		for (k = 0; k < cell->colspan; k++) {
			p += table->cols_widths[i + k] +
			     (k && get_vline_width(table, i + k) >= 0);
		}

		get_cell_width(cell->start, cell->end, table->cellpadding, p, 1, &cell->width,
			       NULL, cell->link_num, NULL);

		int_upper_bound(&cell->width, p);
	}

	colspan = 1;
	do {
		int new_colspan = MAXINT;

		for (i = 0; i < table->x; i++) for (j = 0; j < table->y; j++) {
			struct table_cell *cell = CELL(table, i, j);

			if (!cell->start) continue;

			assertm(cell->colspan + i <= table->x, "colspan out of table");
			if_assert_failed goto end;

			if (cell->colspan == colspan) {
				int k, p = 0;

				for (k = 1; k < colspan; k++)
					p += (get_vline_width(table, i + k) >= 0);

				dst_width(&widths[i], colspan,
					  cell->width - p,
					  &table->max_cols_widths[i]);

			} else if (cell->colspan > colspan
				   && cell->colspan < new_colspan) {
				new_colspan = cell->colspan;
			}
		}
		colspan = new_colspan;
	} while (colspan != MAXINT);

	width = new_width = 0;
	for (i = 0; i < table->x; i++) {
		width += table->cols_widths[i];
		new_width += widths[i];
	}

	if (new_width > width) {
		/* INTERNAL("new width(%d) is larger than previous(%d)", new_width, width); */
		goto end;
	}

	max = -1;
	for (i = 0; i < table->x; i++)
		if (table->max_cols_widths[i] > max) {
			max = table->max_cols_widths[i];
			max_index = i;
		}

	if (max != -1) {
		widths[max_index] += width - new_width;
		if (widths[max_index] <= table->max_cols_widths[max_index]) {
			mem_free(table->cols_widths);
			table->cols_widths = widths;
			return;
		}
	}

end:
	mem_free(widths);
}
#endif

static void
get_table_heights(struct table *table)
{
	int rowspan;
	register int i, j;

	for (j = 0; j < table->y; j++) {
		for (i = 0; i < table->x; i++) {
			struct table_cell *cell = CELL(table, i, j);
			struct part *part;
			int width = 0, sp;

			if (!cell->is_used || cell->is_spanned) continue;

			for (sp = 0; sp < cell->colspan; sp++) {
				width += table->cols_widths[i + sp] +
				         (sp < cell->colspan - 1 &&
				          get_vline_width(table, i + sp + 1) >= 0);
			}

			part = format_cell(table, i, j, NULL, 2, 2, width);
			if (!part) return;

			cell->height = part->box.height;
			/* DBG("%d, %d.", width, cell->height); */
			mem_free(part);
		}
	}

	rowspan = 1;
	do {
		int new_rowspan = MAXINT;

		for (j = 0; j < table->y; j++) {
			for (i = 0; i < table->x; i++) {
				struct table_cell *cell = CELL(table, i, j);

				if (!cell->is_used || cell->is_spanned) continue;

				if (cell->rowspan == rowspan) {
					register int k, p = 0;

					for (k = 1; k < rowspan; k++)
						p += (get_hline_width(table, j + k) >= 0);

					dst_width(&table->rows_heights[j], rowspan,
						  cell->height - p, NULL);

				} else if (cell->rowspan > rowspan &&
					   cell->rowspan < new_rowspan) {
					new_rowspan = cell->rowspan;
				}

			}
		}
		rowspan = new_rowspan;
	} while (rowspan != MAXINT);

	{
		struct table_frames table_frames;

		get_table_frames(table, &table_frames);
		table->real_height = table_frames.top + table_frames.bottom;
		for (j = 0; j < table->y; j++) {
			table->real_height += table->rows_heights[j] +
				 (j && get_hline_width(table, j) >= 0);
		}
	}
}

/* FIXME: too long, split it. */
static void
display_complicated_table(struct table *table, int x, int y, int *yy)
{
	register int i, j;
	struct document *document = table->part->document;
	int xp, yp;
	int expand_cols = (global_doc_opts && global_doc_opts->table_expand_cols);
	color_t default_bgcolor = par_format.bgcolor;
	struct table_frames table_frames;

	get_table_frames(table, &table_frames);

	if (table->fragment_id)
		add_fragment_identifier(table->part, table->fragment_id);

	xp = x + table_frames.left;
	for (i = 0; i < table->x; i++) {
		yp = y + table_frames.top;

		for (j = 0; j < table->y; j++) {
			struct table_cell *cell = CELL(table, i, j);
			int row_height = table->rows_heights[j] +
				(j < table->y - 1 && get_hline_width(table, j + 1) >= 0);
			int row;

			par_format.bgcolor = default_bgcolor;
			for (row = table->part->cy;
			     row < yp + row_height + table_frames.top;
			     row++) {
				expand_lines(table->part, row);
				expand_line(table->part, row, x - 1);
			}

			if (cell->start) {
				int xw = 0;
				int yw = 0;
				register int s;
				struct html_element *state;

				for (s = 0; s < cell->colspan; s++) {
					xw += table->cols_widths[i + s] +
					      (s < cell->colspan - 1 &&
					       get_vline_width(table, i + s + 1) >= 0);
				}

				for (s = 0; s < cell->rowspan; s++) {
					yw += table->rows_heights[j + s] +
					      (s < cell->rowspan - 1 &&
					       get_hline_width(table, j + s + 1) >= 0);
				}

				if (expand_cols) {
					/* This is not working correctly. Some
					 * pages will be rendered much better
					 * (/.) while other will look very ugly
					 * and broken. */
					par_format.bgcolor = table->bgcolor;
					for (s = yp; s < yp + yw; s++) {
						expand_lines(table->part, s);
						expand_line(table->part, s, xp - 1);
					}
				}

				state = init_html_parser_state(ELEMENT_DONT_KILL,
							       par_format.align, 0, 0);

				if (cell->is_header) format.attr |= AT_BOLD;

				format.bg = cell->bgcolor;
				par_format.bgcolor = cell->bgcolor;
 				{
					struct part *part;
					int tmpy = yp;

					if (cell->valign == VALIGN_MIDDLE)
						tmpy += (yw - cell->height)>>1;
					else if (cell->valign == VALIGN_BOTTOM)
						tmpy += (yw - cell->height);

				   	part = format_cell(table, i, j, document, xp, tmpy, xw);
					if (part) {
						int yt;

						for (yt = 0; yt < part->box.height; yt++) {
							expand_lines(table->part, yp + yt);
							expand_line(table->part, yp + yt,
								    xp + table->cols_widths[i]);
						}

						if (cell->fragment_id)
							add_fragment_identifier(part, cell->fragment_id);

						mem_free(part);
					}
				}

				done_html_parser_state(state);
			}

			yp += table->rows_heights[j] +
			      (j < table->y - 1 && get_hline_width(table, j + 1) >= 0);
		}

		if (i < table->x - 1) {
			xp += table->cols_widths[i] + (get_vline_width(table, j + 1) >= 0);
		}
	}

	yp = y;
	for (j = 0; j < table->y; j++) {
		yp += table->rows_heights[j] +
		      (j < table->y - 1 && get_hline_width(table, j + 1) >= 0);
	}

	*yy = yp + table_frames.top + table_frames.bottom;
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

	draw_frame_hchars(table->part, x, y, 1, border_chars[pos], bgcolor, fgcolor);
}

static inline void
draw_frame_hline(struct table *table, signed char *frame[2], int x, int y,
		 int i, int j, color_t bgcolor, color_t fgcolor)
{
 	static unsigned char hltable[] = { ' ', BORDER_SHLINE, BORDER_DHLINE };
 	int pos = H_FRAME_POSITION(table, i, j);

 	assertm(pos < 3, "Horizontal table position out of bound [%d]", pos);
	if_assert_failed return;

 	if (pos < 0 || table->cols_widths[i] <= 0) return;

 	draw_frame_hchars(table->part, x, y, table->cols_widths[i], hltable[pos],
			  bgcolor, fgcolor);
}

static inline void
draw_frame_vline(struct table *table, signed char *frame[2], int x, int y,
		 int i, int j, color_t bgcolor, color_t fgcolor)
{
 	static unsigned char vltable[] = { ' ', BORDER_SVLINE, BORDER_DVLINE };
 	int pos = V_FRAME_POSITION(table, i, j);

 	assertm(pos < 3, "Vertical table position out of bound [%d]", pos);
	if_assert_failed return;

 	if (pos < 0 || table->rows_heights[j] <= 0) return;

 	draw_frame_vchars(table->part, x, y, table->rows_heights[j], vltable[pos],
			  bgcolor, fgcolor);
}

static void
display_table_frames(struct table *table, int x, int y)
{
	struct table_frames table_frames;
 	signed char *frame[2];
  	register int i, j;
  	int cx, cy;
  	int fh_size = (table->x + 2) * (table->y + 1);
  	int fv_size = (table->x + 1) * (table->y + 2);

 	frame[0] = fmem_alloc(fh_size + fv_size);
 	if (!frame[0]) return;
 	memset(frame[0], -1, fh_size + fv_size);

 	frame[1] = &frame[0][fh_size];

	if (table->rules == TABLE_RULE_NONE) goto cont2;

	for (j = 0; j < table->y; j++) for (i = 0; i < table->x; i++) {
		int xsp, ysp;
		struct table_cell *cell = CELL(table, i, j);

		if (!cell->is_used || cell->is_spanned) continue;

		xsp = cell->colspan ? cell->colspan : table->x - i;
		ysp = cell->rowspan ? cell->rowspan : table->y - j;

		if (table->rules != TABLE_RULE_COLS) {
			memset(&H_FRAME_POSITION(table, i, j), table->cellspacing, xsp);
			memset(&H_FRAME_POSITION(table, i, j + ysp), table->cellspacing, xsp);
		}

		if (table->rules != TABLE_RULE_ROWS) {
			memset(&V_FRAME_POSITION(table, i, j), table->cellspacing, ysp);
			memset(&V_FRAME_POSITION(table, i + xsp, j), table->cellspacing, ysp);
		}
	}

	if (table->rules == TABLE_RULE_GROUPS) {
		for (i = 1; i < table->x; i++) {
			if (!table->cols_x[i])
				memset(&V_FRAME_POSITION(table, i, 0), 0, table->y);
		}

		for (j = 1; j < table->y; j++) {
			for (i = 0; i < table->x; i++)
				if (CELL(table, i, j)->group)
					goto cont;

			memset(&H_FRAME_POSITION(table, 0, j), 0, table->x);
cont:;
		}
	}

cont2:

	get_table_frames(table, &table_frames);
	memset(&H_FRAME_POSITION(table, 0, 0), table_frames.top, table->x);
	memset(&H_FRAME_POSITION(table, 0, table->y), table_frames.bottom, table->x);
	memset(&V_FRAME_POSITION(table, 0, 0), table_frames.left, table->y);
	memset(&V_FRAME_POSITION(table, table->x, 0), table_frames.right, table->y);

	cy = y;
	for (j = 0; j <= table->y; j++) {
		cx = x;
		if ((j > 0 && j < table->y && get_hline_width(table, j) >= 0)
		    || (j == 0 && table_frames.top)
		    || (j == table->y && table_frames.bottom)) {
			int w = table_frames.left ? table->border : -1;

			for (i = 0; i < table->x; i++) {
				if (i > 0)
					w = get_vline_width(table, i);

				if (w >= 0) {
					draw_frame_point(table, frame, cx, cy, i, j,
							 par_format.bgcolor, table->bordercolor);
					if (j < table->y)
						draw_frame_vline(table, frame, cx, cy + 1, i, j,
								 par_format.bgcolor, table->bordercolor);
					cx++;
				}

				draw_frame_hline(table, frame, cx, cy, i, j,
						 par_format.bgcolor, table->bordercolor);
				cx += table->cols_widths[i];
			}

			if (table_frames.right) {
				draw_frame_point(table, frame, cx, cy, i, j,
						 par_format.bgcolor, table->bordercolor);
				if (j < table->y)
					draw_frame_vline(table, frame, cx, cy + 1, i, j,
							 par_format.bgcolor, table->bordercolor);
				cx++;
			}

			cy++;

		} else if (j < table->y) {
			for (i = 0; i <= table->x; i++) {
				if ((i > 0 && i < table->x && get_vline_width(table, i) >= 0)
				    || (i == 0 && table_frames.left)
				    || (i == table->x && table_frames.right)) {
					draw_frame_vline(table, frame, cx, cy, i, j,
							 par_format.bgcolor, table->bordercolor);
					cx++;
				}
				if (i < table->x) cx += table->cols_widths[i];
			}
		}

		if (j < table->y) cy += table->rows_heights[j];
		/*for (cyy = cy1; cyy < cy; cyy++) expand_line(table->p, cyy, cx - 1);*/
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
	struct part *part = f;
	struct table *table;
	struct html_start_end *bad_html;
	struct node *node, *new_node;
	unsigned char *al;
	struct html_element *state;
	color_t bgcolor = par_format.bgcolor;
	color_t bordercolor = 0;
	unsigned char *fragment_id;
	int border, cellspacing, vcellpadding, cellpadding, align;
	int frame, rules, width, has_width;
	int cye;
	int x;
	int i;
	int bad_html_n;
	int cpd_pass, cpd_width, cpd_last;
	int margins;

	html_context.table_level++;
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

	has_width = 0;
	width = get_width(attr, "width", (part->document || part->box.x));
	if (width == -1) {
		width = par_format.width - par_format.leftmargin - par_format.rightmargin;
		if (width < 0) width = 0;
		has_width = 1;
	}

	table = parse_table(html, eof, end, bgcolor, (part->document || part->box.x), &bad_html, &bad_html_n);
	if (!table) {
		mem_free_if(bad_html);
		goto ret0;
	}

	for (i = 0; i < bad_html_n; i++) {
		while (bad_html[i].start < bad_html[i].end && isspace(*bad_html[i].start))
			bad_html[i].start++;
		while (bad_html[i].start < bad_html[i].end && isspace(bad_html[i].end[-1]))
			bad_html[i].end--;
		if (bad_html[i].start < bad_html[i].end)
			parse_html(bad_html[i].start, bad_html[i].end, part, NULL);
	}

	mem_free_if(bad_html);

	state = init_html_parser_state(ELEMENT_DONT_KILL, AL_LEFT, 0, 0);

	table->part = part;
	table->border = border;
	table->bordercolor = bordercolor;
	table->cellpadding = cellpadding;
	table->vcellpadding = vcellpadding;
	table->cellspacing = cellspacing;
	table->frame = frame;
	table->rules = rules;
	table->width = width;
	/* table->has_width = has_width; not used. */
	table->fragment_id = fragment_id;
	fragment_id = NULL;

	cpd_pass = 0;
	cpd_last = table->cellpadding;
	cpd_width = 0;  /* not needed, but let the warning go away */

again:
	get_cell_widths(table);
	if (get_column_widths(table)) goto ret2;

	get_table_width(table);

	margins = par_format.leftmargin + par_format.rightmargin;
	if (!part->document && !part->box.x) {
		if (!has_width) int_upper_bound(&table->max_width, width);
		int_lower_bound(&table->max_width, table->min_width);

		int_lower_bound(&part->max_width, table->max_width + margins);
		int_lower_bound(&part->box.width, table->min_width + margins);

		goto ret2;
	}

	if (!cpd_pass && table->min_width > width && table->cellpadding) {
		table->cellpadding = 0;
		cpd_pass = 1;
		cpd_width = table->min_width;
		goto again;
	}
	if (cpd_pass == 1 && table->min_width > cpd_width) {
		table->cellpadding = cpd_last;
		cpd_pass = 2;
		goto again;
	}

	/* DBG("%d %d %d", t->min_width, t->max_width, width); */
	if (table->min_width >= width)
		distribute_widths(table, table->min_width);
	else if (table->max_width < width && has_width)
		distribute_widths(table, table->max_width);
	else
		distribute_widths(table, width);

	if (!part->document && part->box.x == 1) {
		int ww = table->real_width + margins;

		int_bounds(&ww, table->real_width, par_format.width);
		int_lower_bound(&part->box.width, ww);
		part->cy += table->real_height;

		goto ret2;
	}

#ifdef HTML_TABLE_2ND_PASS
	check_table_widths(table);
#endif

	{
		int ww = par_format.width - table->real_width;

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

	get_table_heights(table);

	if (!part->document) {
		int_lower_bound(&part->box.width, table->real_width + margins);
		part->cy += table->real_height;
		goto ret2;
	}

	node = part->document->nodes.next;
	node->box.height = part->box.y - node->box.y + part->cy;

	display_complicated_table(table, x, part->cy, &cye);
	display_table_frames(table, x, part->cy);

	new_node = mem_alloc(sizeof(struct node));
	if (new_node) {
		set_box(&new_node->box, node->box.x, part->box.y + cye,
			node->box.width, 0);
		add_to_list(part->document->nodes, new_node);
	}

	assertm(part->cy + table->real_height == cye, "size does not match; 1:%d, 2:%d",
		part->cy + table->real_height, cye);

	part->cy = cye;
	part->cx = -1;

ret2:
	part->link_num = table->link_num;
	int_lower_bound(&part->box.height, part->cy);
	free_table(table);
	done_html_parser_state(state);

ret0:
	html_context.table_level--;
	mem_free_if(fragment_id);
	if (!html_context.table_level) free_table_cache();
}
