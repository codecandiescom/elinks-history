/* HTML tables parser */
/* $Id: table.c,v 1.5 2004/06/28 22:52:50 pasky Exp $ */

/* Note that this does *not* fit to the HTML parser infrastructure yet, it has
 * some special custom calling conventions and is managed from
 * document/html/tables.c. */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/html/parser/parse.h"
#include "document/html/parser/table.h"
#include "document/html/parser.h"
#include "util/color.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memory.h"


#define INIT_REAL_COLS		2
#define INIT_REAL_ROWS		2

#define realloc_bad_html(bad_html, size) \
	mem_align_alloc(bad_html, size, (size) + 1, struct html_start_end, 0xFF)

static void
get_align(unsigned char *attr, int *a)
{
	unsigned char *al = get_attr_val(attr, "align");

	if (al) {
		if (!(strcasecmp(al, "left"))) *a = ALIGN_LEFT;
		else if (!(strcasecmp(al, "right"))) *a = ALIGN_RIGHT;
		else if (!(strcasecmp(al, "center"))) *a = ALIGN_CENTER;
		else if (!(strcasecmp(al, "justify"))) *a = ALIGN_JUSTIFY;
		else if (!(strcasecmp(al, "char"))) *a = ALIGN_RIGHT; /* NOT IMPLEMENTED */
		mem_free(al);
	}
}

static void
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

static void
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

	table->cells = mem_calloc(INIT_REAL_COLS * INIT_REAL_ROWS,
				  sizeof(struct table_cell));
	if (!table->cells) {
		mem_free(table);
		return NULL;
	}
	table->real_cols = INIT_REAL_COLS;
	table->real_rows = INIT_REAL_ROWS;

	table->columns = mem_calloc(INIT_REAL_COLS, sizeof(struct table_column));
	if (!table->columns) {
		mem_free(table->cells);
		mem_free(table);
		return NULL;
	}
	table->real_columns_count = INIT_REAL_COLS;

	return table;
}

void
free_table(struct table *table)
{
	int col, row;

	mem_free_if(table->min_cols_widths);
	mem_free_if(table->max_cols_widths);
	mem_free_if(table->cols_widths);
	mem_free_if(table->rows_heights);
	mem_free_if(table->fragment_id);
	mem_free_if(table->cols_x);

	for (col = 0; col < table->cols; col++)
		for (row = 0; row < table->rows; row++)
			mem_free_if(CELL(table, col, row)->fragment_id);

	mem_free(table->cells);
	mem_free(table->columns);
	mem_free(table);
}

static void
expand_cells(struct table *table, int dest_col, int dest_row)
{
	if (dest_col >= table->cols) {
		if (table->cols) {
			int last_col = table->cols - 1;
			int row;

			for (row = 0; row < table->rows; row++) {
				int col;
				struct table_cell *cellp = CELL(table, last_col, row);

				if (cellp->colspan != -1) continue;

				for (col = table->cols; col <= dest_col; col++) {
					struct table_cell *cell = CELL(table, col, row);

					cell->is_used = 1;
					cell->is_spanned = 1;
					cell->rowspan = cellp->rowspan;
					cell->colspan = -1;
					cell->col = cellp->col;
					cell->row = cellp->row;
				}
			}
		}
		table->cols = dest_col + 1;
	}

	if (dest_row >= table->rows) {
		if (table->rows) {
			int last_row = table->rows - 1;
			int col;

			for (col = 0; col < table->cols; col++) {
				int row;
				struct table_cell *cellp = CELL(table, col, last_row);

				if (cellp->rowspan != -1) continue;

				for (row = table->rows; row <= dest_row; row++) {
					struct table_cell *cell = CELL(table, col, row);

					cell->is_used = 1;
					cell->is_spanned = 1;
					cell->rowspan = -1;
					cell->colspan = cellp->colspan;
					cell->col = cellp->col;
					cell->row = cellp->row;
				}
			}
		}
		table->rows = dest_row + 1;
	}
}

static struct table_cell *
new_cell(struct table *table, int dest_col, int dest_row)
{
	if (dest_col < table->cols && dest_row < table->rows)
		return CELL(table, dest_col, dest_row);

	while (1) {
		struct table new_table;
		int col, row;

		if (dest_col < table->real_cols && dest_row < table->real_rows) {
			expand_cells(table, dest_col, dest_row);
			return CELL(table, dest_col, dest_row);
		}

		new_table.real_cols = table->real_cols;
		new_table.real_rows = table->real_rows;

		while (dest_col >= new_table.real_cols)
			if (!(new_table.real_cols <<= 1))
				return NULL;
		while (dest_row >= new_table.real_rows)
			if (!(new_table.real_rows <<= 1))
				return NULL;

		new_table.cells = mem_calloc(new_table.real_cols * new_table.real_rows,
					     sizeof(struct table_cell));
		if (!new_table.cells) return NULL;

		for (col = 0; col < table->cols; col++) {
			for (row = 0; row < table->rows; row++) {
				memcpy(CELL(&new_table, col, row),
				       CELL(table, col, row),
				       sizeof(struct table_cell));
			}
		}

		mem_free(table->cells);
		table->cells = new_table.cells;
		table->real_cols = new_table.real_cols;
		table->real_rows = new_table.real_rows;
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
		int i;
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

	while (1) {
		unsigned char *name;
		int namelen;

		while (html < eof
		       && (*html != '<'
		           || parse_element(html, eof, &name, &namelen, NULL,
					    &html)))
			html++;

		if (html >= eof) return eof;

		if (!strlcasecmp(name, namelen, "TABLE", 5)) {
			level++;
		} else if (!strlcasecmp(name, namelen, "/TABLE", 6)) {
			level--;
			if (!level) return html;
		}
	}
}

struct table *
parse_table(unsigned char *html, unsigned char *eof,
	    unsigned char **end, color_t bgcolor,
	    int sh, struct html_start_end **bad_html, int *bhp)
{
	struct table *table;
	struct table_cell *cell;
	unsigned char *t_name, *t_attr, *en;
	unsigned char *lbhp = NULL;
	unsigned char *l_fragment_id = NULL;
	color_t last_bgcolor = bgcolor;
	int t_namelen;
	int in_cell = 0;
	int l_al = ALIGN_LEFT;
	int l_val = VALIGN_MIDDLE;
	int colspan, rowspan;
	int group = 0;
	int i, j, k;
	int c_al = ALIGN_TR, c_val = VALIGN_TR, c_width = WIDTH_AUTO, c_span = 0;
	int cols, rows;
	int col = 0, row = -1;

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
	if (bad_html && !in_cell && !lbhp) {
		if (!realloc_bad_html(bad_html, *bhp)) {
			goto qwe;
		}
		lbhp = html;
		(*bad_html)[(*bhp)++].start = html;
	}

qwe:
	while (html < eof && *html != '<') html++;

	if (html >= eof) {
		if (in_cell) CELL(table, col, row)->end = html;
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
		if (in_cell) CELL(table, col, row)->end = html;
		if (lbhp) (*bad_html)[*bhp-1].end = html;
		goto scan_done;
	}

	if (!strlcasecmp(t_name, t_namelen, "COLGROUP", 8)) {
		if (c_span) new_columns(table, c_span, c_width, c_al, c_val, 1);
		if (lbhp) {
			(*bad_html)[*bhp-1].end = html;
			lbhp = NULL;
		}
		c_al = ALIGN_TR;
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
		c_al = ALIGN_TR;
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

			if (in_cell) {
				CELL(table, col, row)->end = html;
				in_cell = 0;
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

		if (in_cell) {
			CELL(table, col, row)->end = html;
			in_cell = 0;
		}
		if (lbhp) {
			(*bad_html)[*bhp-1].end = html;
			lbhp = NULL;
		}

		if (group) group--;
		l_al = ALIGN_LEFT;
		l_val = VALIGN_MIDDLE;
		last_bgcolor = bgcolor;
		get_align(t_attr, &l_al);
		get_valign(t_attr, &l_val);
		get_bgcolor(t_attr, &last_bgcolor);
		mem_free_set(&l_fragment_id, get_attr_val(t_attr, "id"));
		row++;
		col = 0;
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
	if (in_cell) {
		CELL(table, col, row)->end = html;
		in_cell = 0;
	}

	if (row == -1) {
		row = 0;
		col = 0;
	}

	for (;;col++) {
		cell = new_cell(table, col, row);
		if (!cell) goto see;

		if (!cell->is_used) break;
		if (cell->colspan == -1) goto see;
	}

	in_cell = 1;

	cell->col = col;
	cell->row = row;
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
	if (cell->is_header) cell->align = ALIGN_CENTER;

	if (group == 1) cell->group = 1;

	if (col < table->columns_count) {
		if (table->columns[col].align != ALIGN_TR)
			cell->align = table->columns[col].align;
		if (table->columns[col].valign != VALIGN_TR)
			cell->valign = table->columns[col].valign;
	}

	cell->bgcolor = last_bgcolor;

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
			set_td_width(table, col, width, 0);
	}

	cols = table->cols;
	for (i = 1; colspan != -1 ? i < colspan : i < cols; i++) {
		struct table_cell *span_cell = new_cell(table, col + i, row);

		if (!span_cell || span_cell->is_used) {
			colspan = i;
			for (k = 0; k < i; k++)
				CELL(table, col + k, row)->colspan = colspan;
			break;
		}

		span_cell->is_used = span_cell->is_spanned = 1;
		span_cell->rowspan = rowspan;
		span_cell->colspan = colspan;
		span_cell->col = col;
		span_cell->row = row;
	}

	rows = table->rows;
	for (j = 1; rowspan != -1 ? j < rowspan : j < rows; j++) {
		for (k = 0; k < i; k++) {
			struct table_cell *span_cell = new_cell(table, col + k, row + j);

			if (!span_cell || span_cell->is_used) {
				int l, m;

				if (span_cell->col == col && span_cell->row == row)
					continue;

				for (l = 0; l < k; l++)
					memset(CELL(table, col + l, row + j), 0,
					       sizeof(struct table_cell));

				rowspan = j;

				for (l = 0; l < i; l++)
					for (m = 0; m < j; m++)
						CELL(table, col + l, row + m)->rowspan = j;
				goto see;
			}

			span_cell->is_used = span_cell->is_spanned = 1;
			span_cell->rowspan = rowspan;
			span_cell->colspan = colspan;
			span_cell->col = col;
			span_cell->row = row;
		}
	}

	goto see;

scan_done:
	*end = html;

	mem_free_if(l_fragment_id);

	for (col = 0; col < table->cols; col++) for (row = 0; row < table->rows; row++) {
		struct table_cell *cell = CELL(table, col, row);

		if (!cell->is_spanned) {
			if (cell->colspan == -1) cell->colspan = table->cols - col;
			if (cell->rowspan == -1) cell->rowspan = table->rows - row;
		}
	}

	if (table->rows) {
		table->rows_heights = mem_calloc(table->rows, sizeof(int));
		if (!table->rows_heights) {
			free_table(table);
			return NULL;
		}
	} else table->rows_heights = NULL;

	for (col = 0; col < table->columns_count; col++)
		if (table->columns[col].width != WIDTH_AUTO)
			set_td_width(table, col, table->columns[col].width, 1);
	set_td_width(table, table->cols, WIDTH_AUTO, 0);

	return table;
}
