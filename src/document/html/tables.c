/* HTML tables renderer */
/* $Id: tables.c,v 1.275 2004/06/29 03:07:14 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/html/parser/parse.h"
#include "document/html/parser/table.h"
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

static inline struct part *
format_cell(struct table *table, int col, int row,
	    struct document *document, int x, int y, int width)
{
	struct table_cell *cell = CELL(table, col, row);

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
	       int a, int *min, int *max,
	       int link_num, int *new_link_num)
{
	struct part *part;

	if (min) *min = -1;
	if (max) *max = -1;
	if (new_link_num) *new_link_num = link_num;

	part = format_html_part(start, end, ALIGN_LEFT, cellpadding, width, NULL,
			        !!a, !!a, NULL, link_num);
	if (!part) return;

	if (min) *min = part->box.width;
	if (max) *max = part->max_width;
	if (new_link_num) *new_link_num = part->link_num;

	if (min && max) {
		assertm(*min <= *max, "get_cell_width: %d > %d", *min, *max);
	}

	mem_free(part);
}

#if 0
static inline void
check_cell_widths(struct table *table)
{
	int col, row;

	for (row = 0; row < table->rows; row++) for (col = 0; col < table->cols; col++) {
		int min, max;
		struct table_cell *cell = CELL(table, col, row);

		if (!cell->start) continue;

		get_cell_width(cell->start, cell->end, table->cellpadding, 0, 0,
			       &min, &max, cell->link_num, NULL);

		assertm(!(min != cell->min_width || max < cell->max_width),
			"check_cell_widths failed");
	}
}
#endif

static void
get_cell_widths(struct table *table)
{
	int link_num = table->part->link_num;

	if (!global_doc_opts->table_order) {
		int col, row;

		for (row = 0; row < table->rows; row++)
			for (col = 0; col < table->cols; col++) {
				struct table_cell *cell = CELL(table, col, row);

				if (!cell->start) continue;
				cell->link_num = link_num;
				get_cell_width(cell->start, cell->end, table->cellpadding, 0, 0,
					       &cell->min_width, &cell->max_width,
					       link_num, &link_num);
			}
	} else {
		int col, row;

		for (col = 0; col < table->cols; col++)
			for (row = 0; row < table->rows; row++) {
				struct table_cell *cell = CELL(table, col, row);

				if (!cell->start) continue;
				cell->link_num = link_num;
				get_cell_width(cell->start, cell->end, table->cellpadding, 0, 0,
					       &cell->min_width, &cell->max_width,
					       link_num, &link_num);
			}
	}

	table->link_num = link_num;
}

static inline void
distribute_values(int *values, int count, int wanted, int *limits)
{
	int i;
	int sum = 0, d, r, t;

	for (i = 0; i < count; i++) sum += values[i];
	if (sum >= wanted) return;

again:
	t = wanted - sum;
	d = t / count;
	r = t % count;
	wanted = 0;

	if (limits) {
		for (i = 0; i < count; i++) {
			int delta;

			values[i] += d + (i < r);

			delta = values[i] - limits[i];
			if (delta > 0) {
				wanted += delta;
				values[i] = limits[i];
			}
		}
	} else {
		for (i = 0; i < count; i++) {
			values[i] += d + (i < r);
		}
	}

	if (wanted) {
		assertm(limits, "bug in distribute_values()");
		limits = NULL;
		sum = 0;
		goto again;
	}
}


/* Returns: -1 none, 0, space, 1 line, 2 double */
static inline int
get_vline_width(struct table *table, int col)
{
	int width = 0;

	if (!col) return -1;

	if (table->rules == TABLE_RULE_COLS || table->rules == TABLE_RULE_ALL)
		width = table->cellspacing;
	else if (table->rules == TABLE_RULE_GROUPS)
		width = (col < table->columns_count && table->columns[col].group);

	if (!width && table->cellpadding) width = -1;

	return width;
}

#define has_vline_width(table, col) (get_vline_width(table, col) >= 0)

static int
get_hline_width(struct table *table, int row)
{
	if (!row) return -1;

	if (table->rules == TABLE_RULE_ROWS || table->rules == TABLE_RULE_ALL) {
		if (table->cellspacing || table->vcellpadding)
			return table->cellspacing;
		return -1;

	} else if (table->rules == TABLE_RULE_GROUPS) {
		int col;

		for (col = 0; col < table->cols; col++)
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

	if (!table->cols) return -1; /* prevents calloc(0, sizeof(int)) calls */

	if (!table->min_cols_widths) {
		table->min_cols_widths = mem_calloc(table->cols, sizeof(int));
		if (!table->min_cols_widths) return -1;
	}

	if (!table->max_cols_widths) {
		table->max_cols_widths = mem_calloc(table->cols, sizeof(int));
	   	if (!table->max_cols_widths) {
			mem_free_set(&table->min_cols_widths, NULL);
			return -1;
		}
	}

	if (!table->cols_widths) {
		table->cols_widths = mem_calloc(table->cols, sizeof(int));
		if (!table->cols_widths) {
			mem_free_set(&table->min_cols_widths, NULL);
			mem_free_set(&table->max_cols_widths, NULL);
			return -1;
		}
	}

	colspan = 1;
	do {
		int col, row;
		int new_colspan = INT_MAX;

		for (col = 0; col < table->cols; col++)	for (row = 0; row < table->rows; row++) {
			struct table_cell *cell = CELL(table, col, row);

			if (cell->is_spanned || !cell->is_used) continue;

			assertm(cell->colspan + col <= table->cols, "colspan out of table");
			if_assert_failed return -1;

			if (cell->colspan == colspan) {
				int k, p = 0;

				for (k = 1; k < colspan; k++)
					p += has_vline_width(table, col + k);

				distribute_values(&table->min_cols_widths[col],
						  colspan,
						  cell->min_width - p,
						  &table->max_cols_widths[col]);

				distribute_values(&table->max_cols_widths[col],
						  colspan,
						  cell->max_width - p,
						  NULL);

				for (k = 0; k < colspan; k++) {
					int tmp = col + k;

					int_lower_bound(&table->max_cols_widths[tmp],
							table->min_cols_widths[tmp]);
				}

			} else if (cell->colspan > colspan
				   && cell->colspan < new_colspan) {
				new_colspan = cell->colspan;
			}
		}
		colspan = new_colspan;
	} while (colspan != INT_MAX);

	return 0;
}

static void
get_table_width(struct table *table)
{
	struct table_frames table_frames;
	int min = 0;
	int max = 0;
	int col;

	for (col = 0; col < table->cols; col++) {
		int vl = has_vline_width(table, col);

		min += vl + table->min_cols_widths[col];
		max += vl + table->max_cols_widths[col];
		if (table->cols_x[col] > table->max_cols_widths[col])
			max += table->cols_x[col];
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
	int col;
	int d = width - table->min_width;
	int om = 0;
	char *u;
	int *widths, *max_widths;
	int max_cols_width = 0;
	int cols_size;

	if (!table->cols) return;

	assertm(d >= 0, "too small width %d, required %d", width, table->min_width);

	for (col = 0; col < table->cols; col++)
		int_lower_bound(&max_cols_width, table->max_cols_widths[col]);

	cols_size = table->cols * sizeof(int);
	memcpy(table->cols_widths, table->min_cols_widths, cols_size);
	table->real_width = width;

	/* XXX: We don't need to fail if unsuccessful. See below. --Zas */
	u = fmem_alloc(table->cols);

	widths = fmem_alloc(cols_size);
	if (!widths) goto end;

	max_widths = fmem_alloc(cols_size);
	if (!max_widths) goto end1;

	while (d) {
		int mss, mii;
		int p = 0;
		int wq;
		int dd;

		memset(widths, 0, cols_size);
		memset(max_widths, 0, cols_size);

		for (col = 0; col < table->cols; col++) {
			switch (om) {
				case 0:
					if (table->cols_widths[col] >= table->cols_x[col])
						break;

					widths[col] = 1;
					max_widths[col] = int_min(table->cols_x[col],
								  table->max_cols_widths[col])
							  - table->cols_widths[col];
					if (max_widths[col] <= 0) widths[col] = 0;
					break;
				case 1:
					if (table->cols_x[col] > WIDTH_RELATIVE)
						break;

					widths[col] = WIDTH_RELATIVE - table->cols_x[col];
					max_widths[col] = table->max_cols_widths[col]
							  - table->cols_widths[col];
					if (max_widths[col] <= 0) widths[col] = 0;
					break;
				case 2:
					if (table->cols_x[col] != WIDTH_AUTO)
						break;
					/* Fall-through */
				case 3:
					if (table->cols_widths[col] >= table->max_cols_widths[col])
						break;
					max_widths[col] = table->max_cols_widths[col]
							  - table->cols_widths[col];
					if (max_cols_width) {
						widths[col] = 5 + table->max_cols_widths[col] * 10 / max_cols_width;
					} else {
						widths[col] = 1;
					}
					break;
				case 4:
					if (table->cols_x[col] < 0)
						break;
					widths[col] = 1;
					max_widths[col] = table->cols_x[col]
							  - table->cols_widths[col];
					if (max_widths[col] <= 0) widths[col] = 0;
					break;
				case 5:
					if (table->cols_x[col] >= 0)
						break;
					if (table->cols_x[col] <= WIDTH_RELATIVE) {
						widths[col] = WIDTH_RELATIVE - table->cols_x[col];
					} else {
						widths[col] = 1;
					}
					max_widths[col] = INT_MAX;
					break;
				case 6:
					widths[col] = 1;
					max_widths[col] = INT_MAX;
					break;
				default:
					INTERNAL("could not expand table");
					goto end2;
			}
			p += widths[col];
		}

		if (!p) {
			om++;
			continue;
		}

		wq = 0;
		if (u) memset(u, 0, table->cols);
		dd = d;

again:
		mss = 0;
		mii = -1;
		for (col = 0; col < table->cols; col++) if (widths[col]) {
			int ss;

			if (u && u[col]) continue;
			ss = dd * widths[col] / p;
			if (!ss) ss = 1;
			if (ss > max_widths[col]) ss = max_widths[col];
			if (ss > mss) {
				mss = ss;
				mii = col;
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
			if (d) goto again;
		} else if (!wq) om++;
	}

end2:
	fmem_free(max_widths);

end1:
	fmem_free(widths);

end:
	if (u) fmem_free(u);
}



#ifdef HTML_TABLE_2ND_PASS /* This is by default ON! (<setup.h>) */
static void
check_table_widths(struct table *table)
{
	int col, row;
	int colspan;
	int width, new_width;
	int max, max_index = 0; /* go away, warning! */
	int *widths = mem_calloc(table->cols, sizeof(int));

	if (!widths) return;

	for (row = 0; row < table->rows; row++) for (col = 0; col < table->cols; col++) {
		struct table_cell *cell = CELL(table, col, row);
		int k, p = 0;

		if (!cell->start) continue;

		for (k = 0; k < cell->colspan; k++) {
			p += table->cols_widths[col + k] +
			     (k && has_vline_width(table, col + k));
		}

		get_cell_width(cell->start, cell->end, table->cellpadding, p, 1, &cell->width,
			       NULL, cell->link_num, NULL);

		int_upper_bound(&cell->width, p);
	}

	colspan = 1;
	do {
		int new_colspan = INT_MAX;

		for (col = 0; col < table->cols; col++) for (row = 0; row < table->rows; row++) {
			struct table_cell *cell = CELL(table, col, row);

			if (!cell->start) continue;

			assertm(cell->colspan + col <= table->cols, "colspan out of table");
			if_assert_failed goto end;

			if (cell->colspan == colspan) {
				int k, p = 0;

				for (k = 1; k < colspan; k++)
					p += has_vline_width(table, col + k);

				distribute_values(&widths[col],
						  colspan,
						  cell->width - p,
						  &table->max_cols_widths[col]);

			} else if (cell->colspan > colspan
				   && cell->colspan < new_colspan) {
				new_colspan = cell->colspan;
			}
		}
		colspan = new_colspan;
	} while (colspan != INT_MAX);

	width = new_width = 0;
	for (col = 0; col < table->cols; col++) {
		width += table->cols_widths[col];
		new_width += widths[col];
	}

	if (new_width > width) {
		/* INTERNAL("new width(%d) is larger than previous(%d)", new_width, width); */
		goto end;
	}

	max = -1;
	for (col = 0; col < table->cols; col++)
		if (table->max_cols_widths[col] > max) {
			max = table->max_cols_widths[col];
			max_index = col;
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
	int col, row;

	for (row = 0; row < table->rows; row++) {
		for (col = 0; col < table->cols; col++) {
			struct table_cell *cell = CELL(table, col, row);
			struct part *part;
			int width = 0, sp;

			if (!cell->is_used || cell->is_spanned) continue;

			for (sp = 0; sp < cell->colspan; sp++) {
				width += table->cols_widths[col + sp] +
				         (sp < cell->colspan - 1 &&
				          has_vline_width(table, col + sp + 1));
			}

			part = format_cell(table, col, row, NULL, 2, 2, width);
			if (!part) return;

			cell->height = part->box.height;
			/* DBG("%d, %d.", width, cell->height); */
			mem_free(part);
		}
	}

	rowspan = 1;
	do {
		int new_rowspan = INT_MAX;

		for (row = 0; row < table->rows; row++) {
			for (col = 0; col < table->cols; col++) {
				struct table_cell *cell = CELL(table, col, row);

				if (!cell->is_used || cell->is_spanned) continue;

				if (cell->rowspan == rowspan) {
					int k, p = 0;

					for (k = 1; k < rowspan; k++)
						p += (get_hline_width(table, row + k) >= 0);

					distribute_values(&table->rows_heights[row],
							  rowspan,
							  cell->height - p,
							  NULL);

				} else if (cell->rowspan > rowspan &&
					   cell->rowspan < new_rowspan) {
					new_rowspan = cell->rowspan;
				}

			}
		}
		rowspan = new_rowspan;
	} while (rowspan != INT_MAX);

	{
		struct table_frames table_frames;

		get_table_frames(table, &table_frames);
		table->real_height = table_frames.top + table_frames.bottom;
		for (row = 0; row < table->rows; row++) {
			table->real_height += table->rows_heights[row] +
				 (row && get_hline_width(table, row) >= 0);
		}
	}
}

/* FIXME: too long, split it. */
static void
display_complicated_table(struct table *table, int x, int y)
{
	int col, row;
	struct document *document = table->part->document;
	int xp, yp;
	int expand_cols = (global_doc_opts && global_doc_opts->table_expand_cols);
	color_t default_bgcolor = par_format.bgcolor;
	struct table_frames table_frames;

	get_table_frames(table, &table_frames);

	if (table->fragment_id)
		add_fragment_identifier(table->part, table->fragment_id);

	xp = x + table_frames.left;
	for (col = 0; col < table->cols; col++) {
		yp = y + table_frames.top;

		for (row = 0; row < table->rows; row++) {
			struct table_cell *cell = CELL(table, col, row);
			int row_height = table->rows_heights[row] +
				(row < table->rows - 1 && get_hline_width(table, row + 1) >= 0);
			int row2;

			par_format.bgcolor = default_bgcolor;
			for (row2 = table->part->cy;
			     row2 < yp + row_height + table_frames.top;
			     row2++) {
				expand_lines(table->part, row2);
				expand_line(table->part, row2, x - 1);
			}

			if (cell->start) {
				int xw = 0;
				int yw = 0;
				int s;
				struct html_element *state;

				for (s = 0; s < cell->colspan; s++) {
					xw += table->cols_widths[col + s] +
					      (s < cell->colspan - 1 &&
					       has_vline_width(table, col + s + 1));
				}

				for (s = 0; s < cell->rowspan; s++) {
					yw += table->rows_heights[row + s] +
					      (s < cell->rowspan - 1 &&
					       get_hline_width(table, row + s + 1) >= 0);
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

				   	part = format_cell(table, col, row, document, xp, tmpy, xw);
					if (part) {
						int yt;

						for (yt = 0; yt < part->box.height; yt++) {
							expand_lines(table->part, yp + yt);
							expand_line(table->part, yp + yt,
								    xp + table->cols_widths[col]);
						}

						if (cell->fragment_id)
							add_fragment_identifier(part, cell->fragment_id);

						mem_free(part);
					}
				}

				done_html_parser_state(state);
			}

			yp += table->rows_heights[row] +
			      (row < table->rows - 1 && get_hline_width(table, row + 1) >= 0);
		}

		if (col < table->cols - 1) {
			xp += table->cols_widths[col] + has_vline_width(table, col + 1);
		}
	}

	yp = y;
	for (row = 0; row < table->rows; row++) {
		yp += table->rows_heights[row] +
		      (row < table->rows - 1 && get_hline_width(table, row + 1) >= 0);
	}

	assertm(table->part->cy + table->real_height
		==
		yp + table_frames.top + table_frames.bottom,
		"size does not match; 1:%d, 2:%d",
		table->part->cy + table->real_height,
		yp + table_frames.top + table_frames.bottom);

}


static inline int
get_frame_pos(int a, int a_size, int b, int b_size)
{
	assert(a >= -1 || a < a_size + 2 || b >= 0 || b <= b_size);
	if_assert_failed return 0;
	return a + 1 + (a_size + 2) * b;
}

#define H_FRAME_POSITION(table, col, row) frame[0][get_frame_pos(col, (table)->cols, row, (table)->rows)]
#define V_FRAME_POSITION(table, col, row) frame[1][get_frame_pos(row, (table)->rows, col, (table)->cols)]

static inline void
draw_frame_point(struct table *table, signed char *frame[2], int x, int y,
		 int col, int row, color_t bgcolor, color_t fgcolor)
{
	/* TODO: Use /BORDER._.* / macros ! --pasky */
	static unsigned char const border_chars[81] = {
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
	signed char left   = H_FRAME_POSITION(table, col - 1,     row);
	signed char right  = H_FRAME_POSITION(table,     col,     row);
	signed char top    = V_FRAME_POSITION(table,     col, row - 1);
	signed char bottom = V_FRAME_POSITION(table,     col,     row);
	int pos;

	if (left < 0 && right < 0 && top < 0 && bottom < 0) return;

	pos =      int_max(top,    0)
	    +  3 * int_max(right,  0)
	    +  9 * int_max(left,   0)
	    + 27 * int_max(bottom, 0);

	draw_frame_hchars(table->part, x, y, 1, border_chars[pos], bgcolor, fgcolor);
}

static inline void
draw_frame_hline(struct table *table, signed char *frame[2], int x, int y,
		 int col, int row, color_t bgcolor, color_t fgcolor)
{
 	static unsigned char const hltable[] = { ' ', BORDER_SHLINE, BORDER_DHLINE };
 	int pos = H_FRAME_POSITION(table, col, row);

 	assertm(pos < 3, "Horizontal table position out of bound [%d]", pos);
	if_assert_failed return;

 	if (pos < 0 || table->cols_widths[col] <= 0) return;

 	draw_frame_hchars(table->part, x, y, table->cols_widths[col], hltable[pos],
			  bgcolor, fgcolor);
}

static inline void
draw_frame_vline(struct table *table, signed char *frame[2], int x, int y,
		 int col, int row, color_t bgcolor, color_t fgcolor)
{
 	static unsigned char const vltable[] = { ' ', BORDER_SVLINE, BORDER_DVLINE };
 	int pos = V_FRAME_POSITION(table, col, row);

 	assertm(pos < 3, "Vertical table position out of bound [%d]", pos);
	if_assert_failed return;

 	if (pos < 0 || table->rows_heights[row] <= 0) return;

 	draw_frame_vchars(table->part, x, y, table->rows_heights[row], vltable[pos],
			  bgcolor, fgcolor);
}

static void
display_table_frames(struct table *table, int x, int y)
{
	struct table_frames table_frames;
 	signed char *frame[2];
  	int col, row;
  	int cx, cy;
  	int fh_size = (table->cols + 2) * (table->rows + 1);
  	int fv_size = (table->cols + 1) * (table->rows + 2);

 	frame[0] = fmem_alloc(fh_size + fv_size);
 	if (!frame[0]) return;
 	memset(frame[0], -1, fh_size + fv_size);

 	frame[1] = &frame[0][fh_size];

	if (table->rules == TABLE_RULE_NONE) goto cont2;

	for (row = 0; row < table->rows; row++) for (col = 0; col < table->cols; col++) {
		int xsp, ysp;
		struct table_cell *cell = CELL(table, col, row);

		if (!cell->is_used || cell->is_spanned) continue;

		xsp = cell->colspan ? cell->colspan : table->cols - col;
		ysp = cell->rowspan ? cell->rowspan : table->rows - row;

		if (table->rules != TABLE_RULE_COLS) {
			memset(&H_FRAME_POSITION(table, col, row), table->cellspacing, xsp);
			memset(&H_FRAME_POSITION(table, col, row + ysp), table->cellspacing, xsp);
		}

		if (table->rules != TABLE_RULE_ROWS) {
			memset(&V_FRAME_POSITION(table, col, row), table->cellspacing, ysp);
			memset(&V_FRAME_POSITION(table, col + xsp, row), table->cellspacing, ysp);
		}
	}

	if (table->rules == TABLE_RULE_GROUPS) {
		for (col = 1; col < table->cols; col++) {
			if (!table->cols_x[col])
				memset(&V_FRAME_POSITION(table, col, 0), 0, table->rows);
		}

		for (row = 1; row < table->rows; row++) {
			for (col = 0; col < table->cols; col++)
				if (CELL(table, col, row)->group)
					goto cont;

			memset(&H_FRAME_POSITION(table, 0, row), 0, table->cols);
cont:;
		}
	}

cont2:

	get_table_frames(table, &table_frames);
	memset(&H_FRAME_POSITION(table, 0, 0), table_frames.top, table->cols);
	memset(&H_FRAME_POSITION(table, 0, table->rows), table_frames.bottom, table->cols);
	memset(&V_FRAME_POSITION(table, 0, 0), table_frames.left, table->rows);
	memset(&V_FRAME_POSITION(table, table->cols, 0), table_frames.right, table->rows);

	cy = y;
	for (row = 0; row <= table->rows; row++) {
		cx = x;
		if ((row > 0 && row < table->rows && get_hline_width(table, row) >= 0)
		    || (row == 0 && table_frames.top)
		    || (row == table->rows && table_frames.bottom)) {
			int w = table_frames.left ? table->border : -1;

			for (col = 0; col < table->cols; col++) {
				if (col > 0)
					w = get_vline_width(table, col);

				if (w >= 0) {
					draw_frame_point(table, frame, cx, cy, col, row,
							 par_format.bgcolor, table->bordercolor);
					if (row < table->rows)
						draw_frame_vline(table, frame, cx, cy + 1, col, row,
								 par_format.bgcolor, table->bordercolor);
					cx++;
				}

				draw_frame_hline(table, frame, cx, cy, col, row,
						 par_format.bgcolor, table->bordercolor);
				cx += table->cols_widths[col];
			}

			if (table_frames.right) {
				draw_frame_point(table, frame, cx, cy, col, row,
						 par_format.bgcolor, table->bordercolor);
				if (row < table->rows)
					draw_frame_vline(table, frame, cx, cy + 1, col, row,
							 par_format.bgcolor, table->bordercolor);
				cx++;
			}

			cy++;

		} else if (row < table->rows) {
			for (col = 0; col <= table->cols; col++) {
				if ((col > 0 && col < table->cols && has_vline_width(table, col))
				    || (col == 0 && table_frames.left)
				    || (col == table->cols && table_frames.right)) {
					draw_frame_vline(table, frame, cx, cy, col, row,
							 par_format.bgcolor, table->bordercolor);
					cx++;
				}
				if (col < table->cols) cx += table->cols_widths[col];
			}
		}

		if (row < table->rows) cy += table->rows_heights[row];
		/*for (cyy = cy1; cyy < cy; cyy++) expand_line(table->p, cyy, cx - 1);*/
	}

	fmem_free(frame[0]);
}

static void
format_bad_table_html(struct table *table)
{
	int i;

	for (i = 0; i < table->bad_html_size; i++) {
		unsigned char *start = table->bad_html[i].start;
		unsigned char *end = table->bad_html[i].end;

		while (start < end && isspace(*start))
			start++;

		while (start < end && isspace(end[-1]))
			end--;

		if (start < end)
			parse_html(start, end, table->part, NULL);
	}
}

void
format_table(unsigned char *attr, unsigned char *html, unsigned char *eof,
	     unsigned char **end, void *f)
{
	struct part *part = f;
	struct table *table;
	struct node *node, *new_node;
	struct html_element *state;
	color_t bgcolor = par_format.bgcolor;
	int x;
	int cpd_pass, cpd_width, cpd_last;
	int margins;

	html_context.table_level++;
	get_bgcolor(attr, &bgcolor);

	table = parse_table(html, eof, end, bgcolor, attr, (part->document || part->box.x));
	if (!table) goto ret0;

	table->part = part;

	format_bad_table_html(table);

	state = init_html_parser_state(ELEMENT_DONT_KILL, ALIGN_LEFT, 0, 0);

	cpd_pass = 0;
	cpd_last = table->cellpadding;
	cpd_width = 0;  /* not needed, but let the warning go away */

again:
	get_cell_widths(table);
	if (get_column_widths(table)) goto ret2;

	get_table_width(table);

	margins = par_format.leftmargin + par_format.rightmargin;
	if (!part->document && !part->box.x) {
		if (!table->full_width)
			int_upper_bound(&table->max_width, table->width);
		int_lower_bound(&table->max_width, table->min_width);

		int_lower_bound(&part->max_width, table->max_width + margins);
		int_lower_bound(&part->box.width, table->min_width + margins);

		goto ret2;
	}

	if (!cpd_pass && table->min_width > table->width && table->cellpadding) {
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

	/* DBG("%d %d %d", t->min_width, t->max_width, table->width); */
	if (table->min_width >= table->width)
		distribute_widths(table, table->min_width);
	else if (table->max_width < table->width && table->full_width)
		distribute_widths(table, table->max_width);
	else
		distribute_widths(table, table->width);

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
		unsigned char *al;
		int ww = par_format.width - table->real_width;
		int align = par_format.align;

		if (align == ALIGN_NONE || align == ALIGN_JUSTIFY) align = ALIGN_LEFT;

		al = get_attr_val(attr, "align");
		if (al) {
			if (!strcasecmp(al, "left")) align = ALIGN_LEFT;
			else if (!strcasecmp(al, "center")) align = ALIGN_CENTER;
			else if (!strcasecmp(al, "right")) align = ALIGN_RIGHT;
			mem_free(al);
		}

		if (align == ALIGN_CENTER)
			x = (ww + par_format.leftmargin
		     	     - par_format.rightmargin) >> 1;
		else if (align == ALIGN_RIGHT)
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

	display_complicated_table(table, x, part->cy);
	display_table_frames(table, x, part->cy);

	part->cy += table->real_height;
	part->cx = -1;

	new_node = mem_alloc(sizeof(struct node));
	if (new_node) {
		set_box(&new_node->box, node->box.x, part->box.y + part->cy,
			node->box.width, 0);
		add_to_list(part->document->nodes, new_node);
	}

ret2:
	part->link_num = table->link_num;
	int_lower_bound(&part->box.height, part->cy);
	free_table(table);
	done_html_parser_state(state);

ret0:
	html_context.table_level--;
	if (!html_context.table_level) free_table_cache();
}
