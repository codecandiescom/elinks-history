/* HTML renderer */
/* $Id: renderer.c,v 1.11 2002/03/27 21:52:22 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdarg.h>
#include <string.h>

#include <links.h>

#include <main.h>
#include <bfu/align.h>
#include <config/default.h>
#include <document/cache.h>
#include <document/options.h>
#include <document/session.h>
#include <document/view.h>
#include <document/html/colors.h>
#include <document/html/parser.h>
#include <document/html/renderer.h>
#include <document/html/tables.h>
#include <intl/charsets.h>
#include <lowlevel/ttime.h>
#include <protocol/http/header.h>
#include <protocol/url.h>
#include <util/error.h>

/* Types and structs */

struct rgb palette[] = {
#if 0
	{0x00, 0x00, 0x00},
	{0x80, 0x00, 0x00},
	{0x00, 0x80, 0x00},
	{0x80, 0x80, 0x00},
	{0x00, 0x00, 0x80},
	{0x80, 0x00, 0x80},
	{0x00, 0x80, 0x80},
	{0xC0, 0xC0, 0xC0},
	{0x80, 0x80, 0x80},
	{0xff, 0x00, 0x00},
	{0x00, 0xff, 0x00},
	{0xff, 0xff, 0x00},
	{0x00, 0x00, 0xff},
	{0xff, 0x00, 0xff},
	{0x00, 0xff, 0xff},
	{0xff, 0xff, 0xff},
#endif
#if 0
	{0x00, 0x00, 0x00},
	{0xaa, 0x00, 0x00},
	{0x00, 0xaa, 0x00},
	{0xaa, 0x55, 0x00},
	{0x00, 0x00, 0xaa},
	{0xaa, 0x00, 0xaa},
	{0x00, 0xaa, 0xaa},
	{0xaa, 0xaa, 0xaa},
	{0x55, 0x55, 0x55},
	{0xff, 0x55, 0x55},
	{0x55, 0xff, 0x55},
	{0xff, 0xff, 0x55},
	{0x55, 0x55, 0xff},
	{0xff, 0x55, 0xff},
	{0x55, 0xff, 0xff},
	{0xff, 0xff, 0xff},
#endif
	{0x00, 0x00, 0x00},
	{0x80, 0x00, 0x00},
	{0x00, 0x80, 0x00},
	{0xaa, 0x55, 0x00},
	{0x00, 0x00, 0x80},
	{0x80, 0x00, 0x80},
	{0x00, 0x80, 0x80},
	{0xaa, 0xaa, 0xaa},
	{0x55, 0x55, 0x55},
	{0xff, 0x55, 0x55},
	{0x55, 0xff, 0x55},
	{0xff, 0xff, 0x55},
	{0x55, 0x55, 0xff},
	{0xff, 0x55, 0xff},
	{0x55, 0xff, 0xff},
	{0xff, 0xff, 0xff},
	{-1, -1, -1}
};

struct rgb_cache_entry {
	int color;
	int l;
	struct rgb rgb;
};

struct table_cache_entry {
	struct table_cache_entry *next;
	struct table_cache_entry *prev;
	unsigned char *start;
	unsigned char *end;
	int align;
	int m;
	int width;
	int xs;
	int link_num;
	struct part part;
};


/* Global variables */
int last_link_to_move;
struct tag *last_tag_to_move;
struct tag *last_tag_for_newline;
unsigned char *last_link;
unsigned char *last_target;
unsigned char *last_image;
struct form_control *last_form;
int nobreak;
struct conv_table *convert_table;
int g_ctrl_num;
int margin;
int empty_format;
int format_cache_entries = 0;

struct list_head table_cache = { &table_cache, &table_cache };
struct list_head format_cache = {&format_cache, &format_cache};


/* Prototypes */
void line_break(struct part *);
void put_chars(struct part *, unsigned char *, int);


#if 0
struct rgb rgbcache = {0, 0, 0};
int rgbcache_c = 0;

static inline int find_nearest_color(struct rgb *r, int l)
{
	int dist, dst, min, i;
	if (r->r == rgbcache.r && r->g == rgbcache.g && r->b == rgbcache.b) return rgbcache_c;
	dist = 0xffffff;
	min = 0;
	for (i = 0; i < l; i++) if ((dst = color_distance(r, &palette[i])) < dist)
		dist = dst, min = i;
	return min;
}
#endif

/* color_distance() */
static inline int color_distance(struct rgb *c1, struct rgb *c2)
{
	return 3 * (c1->r - c2->r) * (c1->r - c2->r) +
	       4 * (c1->g - c2->g) * (c1->g - c2->g) +
	       2 * (c1->b - c2->b) * (c1->b - c2->b);
}

#define RGB_HASH_SIZE 4096

#define HASH_RGB(r, l) ((((r)->r << 3) + ((r)->g << 2) + (r)->b + (l)) & (RGB_HASH_SIZE - 1))

/* find_nearest_color() */
static inline int find_nearest_color(struct rgb *r, int l)
{
	int dist, dst, min, i;
	static struct rgb_cache_entry rgb_cache[RGB_HASH_SIZE];
	static int cache_init = 0;
	int h;

	if (!cache_init) {
		for (h = 0; h < RGB_HASH_SIZE; h++)
			rgb_cache[h].color = -1;
		cache_init = 1;
	}

	h = HASH_RGB(r, l);

	if (rgb_cache[h].color != -1 && rgb_cache[h].l == l
			&& rgb_cache[h].rgb.r == r->r && rgb_cache[h].rgb.g == r->g
			&& rgb_cache[h].rgb.b == r->b) return rgb_cache[h].color;

	dist = 0xffffff;
	min = 0;

	for (i = 0; i < l; i++) {
		dst = color_distance(r, &palette[i]);
		if (dst < dist) {
			dist = dst;
			min = i;
		}
	}

	rgb_cache[h].color = min;
	rgb_cache[h].l = l;
	rgb_cache[h].rgb.r = r->r;
	rgb_cache[h].rgb.g = r->g;
	rgb_cache[h].rgb.b = r->b;
	return min;
}

/* fg_color() */
static inline int fg_color(int fg, int bg)
{
	/* 0 == brightgrey  6 == cyan        12 == brightblue
	 * 1 == red         7 == brightgrey  13 == brightmagenta
	 * 2 == green       8 == black       14 == brightcyan
	 * 3 == red         9 == brightred   15 == brightwhite
	 * 4 == blue       10 == brightgreen
	 * 5 == magenta    11 == brightyellow
	 */

	/* This looks like it should be more efficient. It results in
	 * different machine-code, but the same number of instructions:
	 * int l, h;
	 * if (bg < fg) l = bg, h = fg; else l = fg, h = bg;
	 */
	int l = bg < fg ? bg : fg;
	int h = bg < fg ? fg : bg;

	/* Below, I changed !l to l == 0. The two forms compile to the same
	 * machine-code with GCC 2.95.4. The latter is more readable (IMO). */

	if (l == h
		/* Check for clashing colours. For example, 3 (red) clashes
		 * with 5 (magenta) and 12 (brightblue). */
		|| (l == 0 && (h == 8))
		|| (l == 1 && (h == 3 || h == 5 || h == 12))
		|| (l == 2 && (h == 6))
		|| (l == 3 && (h == 5 || h == 12))
		|| ((l == 4 || l == 5) && (h == 8 || h == 12))
		|| (d_opt->avoid_dark_on_black && /* FIXME: when possibility to change bg color... */
			   ((l == 0 && (h == 4 || h == 12)) ||
			    (l == 1 && (h == 8)))
		   )
	   )
		return (fg == 4 || fg == 12)
			&& (bg == 0 || bg == 8) ? 6
						: (7 - 7 * (bg == 2 ||
							    bg == 6 ||
							    bg == 7));

	return fg;
}

#ifdef ALIGN
#undef ALIGN
#endif

#define ALIGN(x) (((x)+0x7f)&~0x7f)


/* realloc_lines() */
static int realloc_lines(struct part *p, int y)
{
	int i;

	if (ALIGN(y + 1) >= ALIGN(p->data->y)) {
		struct line *l;

		l = mem_realloc(p->data->data, ALIGN(y+1)*sizeof(struct line));
		if (!l)	return -1;

		p->data->data = l;
	}

	for (i = p->data->y; i <= y; i++) {
		p->data->data[i].l = 0;
		p->data->data[i].c = p->bgcolor;
		p->data->data[i].d = DUMMY;
	}

	p->data->y = i;

	return 0;
}


/* realloc_line() */
static int realloc_line(struct part *p, int y, int x)
{
	int i;

	if (ALIGN(x + 1) >= ALIGN(p->data->data[y].l)) {
		chr *l;

		l = mem_realloc(p->data->data[y].d, ALIGN(x+1)*sizeof(chr));
		if (!l)	return -1;

		p->data->data[y].d = l;
	}

	for (i = p->data->data[y].l; i <= x; i++) {
		p->data->data[y].d[i] = (p->data->data[y].c << 11) | ' ';
	}

	p->data->data[y].c = p->bgcolor;
	p->data->data[y].l = i;

	return 0;
}

#undef ALIGN


/* xpand_lines() */
static inline int xpand_lines(struct part *p, int y)
{
	/*if (y >= p->y) p->y = y + 1;*/
	if (!p->data) return 0;
	y += p->yp;
	if (y >= p->data->y) return realloc_lines(p, y);

	return 0;
}


/* expand_lines() */
int expand_lines(struct part *part, int y)
{
	return xpand_lines(part, y);
}


/* xpand_line() */
static inline int xpand_line(struct part *p, int y, int x)
{
	if (!p->data) return 0; /* !!! FIXME: p->x (?) */
	x += p->xp;
	y += p->yp;
#ifdef DEBUG
	if (y >= p->data->y) {
		internal("line does not exist");
		return -1;
	}
#endif
	if (x >= p->data->data[y].l) return realloc_line(p, y, x);
	return 0;
}


/* expand_line() */
int expand_line(struct part *part, int y, int x)
{
	return xpand_line(part, y, x);
}


/* realloc_spaces() */
int realloc_spaces(struct part *p, int l)
{
	unsigned char *c;

	c = mem_realloc(p->spaces, l + 1);
	if (!c) return -1;
	memset(c + p->spl, 0, l - p->spl + 1);

	p->spl = l + 1;
	p->spaces = c;

	return 0;
}


/* xpand_spaces() */
static inline int xpand_spaces(struct part *p, int l)
{
	if (l >= p->spl) return realloc_spaces(p, l);
	return 0;
}


#define POS(x, y) (part->data->data[part->yp + (y)].d[part->xp + (x)])
#define LEN(y) (part->data->data[part->yp + (y)].l - part->xp < 0 ? 0 : part->data->data[part->yp + (y)].l - part->xp)
#define SLEN(y, x) part->data->data[part->yp + (y)].l = part->xp + x;
#define X(x) (part->xp + (x))
#define Y(y) (part->yp + (y))

/* set_hchar() */
static inline void set_hchar(struct part *part, int x, int y, unsigned c)
{
	if (xpand_lines(part, y)) return;
	if (xpand_line(part, y, x)) return;
	POS(x, y) = c;
}


/* set_hchars() */
static inline void set_hchars(struct part *part, int x, int y, int xl, unsigned c)
{
	if (xpand_lines(part, y)) return;
	if (xpand_line(part, y, x + xl - 1)) return;
	for (; xl; xl--, x++) POS(x, y) = c;
}


/* xset_hchar() */
void xset_hchar(struct part *part, int x, int y, unsigned c)
{
	set_hchar(part, x, y, c);
}


/* xset_hchars() */
void xset_hchars(struct part *part, int x, int y, int xl, unsigned c)
{
	set_hchars(part, x, y, xl, c);
}


/* set_hline() */
static inline void set_hline(struct part *part, int x, int y,int xl,
                             unsigned char *d, unsigned c, int spc)
{
	if (xpand_lines(part, y)) return;
	if (xpand_line(part, y, x+xl-1)) return;
	if (spc && xpand_spaces(part, x+xl-1)) return;

	for (; xl; xl--, x++, d++) {
		if (spc) part->spaces[x] = *d == ' ';
		if (part->data) POS(x, y) = *d | c;
	}
}


/* move_links() */
static inline void move_links(struct part *part, int xf, int yf, int xt, int yt)
{
	struct tag *tag;
	int nlink;
	int matched = 0;

	if (!part->data) return;
	xpand_lines(part, yt);

	for (nlink = last_link_to_move; nlink < part->data->nlinks; nlink++) {
		struct link *link = &part->data->links[nlink];
		int i;

		for (i = 0; i < link->n; i++) {
			if (link->pos[i].y == Y(yf)) {
				matched = 1;
				if (link->pos[i].x >= X(xf)) {
					if (yt >= 0) {
						link->pos[i].y = Y(yt);
						link->pos[i].x += -xf + xt;
					} else {
						memmove(&link->pos[i],
							&link->pos[i + 1],
							(link->n - i - 1) *
							sizeof(struct point));
						link->n--;
						i--;
					}
				}
			}
		}

#if 0
		if (!link->n) {
			if (link->where) mem_free(link->where);
			if (link->target) mem_free(link->target);
			if (link->where_img) mem_free(link->where_img);
			if (link->pos) mem_free(link->pos);
			memmove(link, link + 1, (part->data->nlinks - nlink - 1) * sizeof(struct link));
			part->data->nlinks --;
			nlink--;
		}
#endif

		if (!matched /* && nlink >= 0 */) last_link_to_move = nlink;
	}

	matched = 0;

	if (yt >= 0) {
		for (tag = last_tag_to_move->next;
	   			(void *) tag != &part->data->tags;
		     tag = tag->next) {
			if (tag->y == Y(yf)) {
				matched = 1;
				if (tag->x >= X(xf)) {
					tag->y = Y(yt), tag->x += -xf + xt;
				}
			}
			if (!matched) last_tag_to_move = tag;
		}
	}
}


/* copy_chars() */
static inline void copy_chars(struct part *part, int x, int y, int xl, chr *d)
{
	if (xl <= 0) return;
	if (xpand_lines(part, y)) return;
	if (xpand_line(part, y, x+xl-1)) return;
	for (; xl; xl--, x++, d++) POS(x, y) = *d;
}


/* move_chars() */
static inline void move_chars(struct part *part, int x, int y, int nx, int ny)
{
	if (LEN(y) - x <= 0) return;
	copy_chars(part, nx, ny, LEN(y) - x, &POS(x, y));
	SLEN(y, x);
	move_links(part, x, y, nx, ny);
}


/* shift_chars() */
static inline void shift_chars(struct part *part, int y, int shift)
{
	chr *a;
	int len = LEN(y);

	a = mem_alloc(len * sizeof(chr));
	if (!a) return;

	memcpy(a, &POS(0, y), len * sizeof(chr));
	set_hchars(part, 0, y, shift, (part->data->data[y].c << 11) | ' ');
	copy_chars(part, shift, y, len, a);
	mem_free(a);

	move_links(part, 0, y, shift, y);
}


/* del_chars() */
static inline void del_chars(struct part *part, int x, int y)
{
	SLEN(y, x);
	move_links(part, x, y, -1, -1);
}

#define overlap(x) ((x).width - (x).rightmargin > 0 ? (x).width - (x).rightmargin : 0)


/* split_line() */
int split_line(struct part *part)
{
	int i;

#if 0
	if (!part->data) goto r;
	printf("split: %d,%d   , %d,%d,%d\n",part->cx,part->cy,par_format.rightmargin,par_format.leftmargin,part->cx);
#endif

	for (i = overlap(par_format); i >= par_format.leftmargin; i--)
		if (i < part->spl && part->spaces[i])
			goto split;

#if 0
	for (i = part->cx - 1; i > overlap(par_format) && i > par_format.leftmargin; i--)
#endif

	for (i = par_format.leftmargin; i < part->cx ; i++)
		if (i < part->spl && part->spaces[i])
			goto split;

#if 0
	for (i = overlap(par_format); i >= par_format.leftmargin; i--)
		if ((POS(i, part->cy) & 0xff) == ' ')
			goto split;
	for (i = part->cx - 1; i > overlap(par_format) && i > par_format.leftmargin; i--)
		if ((POS(i, part->cy) & 0xff) == ' ')
			goto split;
#endif

	if (part->cx + par_format.rightmargin > part->x) part->x = part->cx + par_format.rightmargin;

#if 0
	if (part->y < part->cy + 1) part->y = part->cy + 1;
	part->cy++; part->cx = -1;
	memset(part->spaces, 0, part->spl);
	if (part->data) xpand_lines(part, part->cy + 1);
	line_break(part);
#endif

	return 0;

split:
	if (i + par_format.rightmargin > part->x)
		part->x = i + par_format.rightmargin;
	if (part->data) {
#ifdef DEBUG
		if ((POS(i, part->cy) & 0xff) != ' ')
			internal("bad split: %c", (char)POS(i, part->cy));
#endif
		move_chars(part, i+1, part->cy, par_format.leftmargin, part->cy+1);
		del_chars(part, i, part->cy);
	}

	memmove(part->spaces, part->spaces + i + 1, part->spl - i - 1);
	memset(part->spaces + part->spl - i - 1, 0, i + 1);
	memmove(part->spaces + par_format.leftmargin, part->spaces, part->spl - par_format.leftmargin);
	part->cy++; part->cx -= i - par_format.leftmargin + 1;

	/*return 1 + (part->cx == par_format.leftmargin);*/

	if (part->cx == par_format.leftmargin) part->cx = -1;
	if (part->y < part->cy + (part->cx != -1)) part->y = part->cy + (part->cx != -1);

	return 1 + (part->cx == -1);
}

/* justify_line() */
/* This function is very rare exemplary of clean and beautyful code here.
 * Please handle with care. --pasky */
static void justify_line(struct part *part, int y)
{
	chr *line; /* we save original line here */
	int len = LEN(y);
	int pos;
	int *space_list;
	int spaces;

	line = mem_alloc(len * sizeof(chr));
	if (!line) return;

	/* It may sometimes happen that the line is only one char long and that
	 * char is space - then we're going to write to both [0] and [1], but
	 * we allocated only one field. Thus, we've to do (len + 1). --pasky */
	space_list = mem_alloc((len + 1) * sizeof(int));
	if (!space_list) return;

	memcpy(line, &POS(0, y), len * sizeof(chr));

	/* Skip leading spaces */

	spaces = 0;
	pos = 0;

	while ((line[pos] & 0xff) == ' ')
		pos++;

	/* Yes, this can be negative, we know. But we add one to it always
	 * anyway, so it's ok. */
	space_list[spaces++] = pos - 1;

	/* Count spaces */

	for (; pos < len; pos++)
		if ((line[pos] & 0xff) == ' ')
			space_list[spaces++] = pos;

	space_list[spaces] = len;

	/* Realign line */

	if (spaces > 1) {
		int insert = overlap(par_format) - len;
		int prev_end = 0;
		int word;

		set_hchars(part, 0, y, overlap(par_format),
			   (part->data->data[y].c << 11) | ' ');

		for (word = 0; word < spaces; word++) {
			/* We have to increase line length by 'insert' num. of
			 * characters, so we move 'word'th word 'word_shift'
			 * characters right. */

			int word_start = space_list[word] + 1;
			int word_len = space_list[word + 1] - word_start;
			int word_shift = (word * insert) / (spaces - 1);
			int new_start = word_start + word_shift;

			copy_chars(part, new_start, y, word_len,
				   &line[word_start]);

			/* There are now (new_start - prev_end) spaces before
			 * the word. */
			if (word != 0)
				move_links(part, prev_end + 1, y, new_start, y);

			prev_end = new_start + word_len;
		}
	}

	mem_free(space_list);
	mem_free(line);
}


/* align_line() */
void align_line(struct part *part, int y, int last)
{
	int shift;
	int len;

	if (!part->data)
		return;

	len = LEN(y);

	if (!len || par_format.align == AL_LEFT ||
		    par_format.align == AL_NO)
		return;

	if (par_format.align == AL_BLOCK) {
		if (!last)
			justify_line(part, y);
		return;
	}

	shift = overlap(par_format) - len;
	if (par_format.align == AL_CENTER)
		shift /= 2;
	if (shift > 0)
		shift_chars(part, y, shift);
}


/* new_link() */
struct link *new_link(struct f_data *f)
{
	if (!f) return NULL;

	if (!(f->nlinks & (ALLOC_GR - 1))) {
		struct link *l = mem_realloc(f->links,
					     (f->nlinks + ALLOC_GR)
					     * sizeof(struct link));

		if (!l) return NULL;
		f->links = l;
	}

	memset(&f->links[f->nlinks], 0, sizeof(struct link));

	return &f->links[f->nlinks++];
}


/* html_tag() */
void html_tag(struct f_data *f, unsigned char *t, int x, int y)
{
	struct tag *tag;

	if (!f) return;

	tag = mem_alloc(sizeof(struct tag) + strlen(t) + 1);
	if (tag) {
		tag->x = x;
		tag->y = y;
		strcpy(tag->name, t);
		add_to_list(f->tags, tag);
		if ((void *) last_tag_for_newline == &f->tags)
			last_tag_for_newline = tag;
	}
}


#define CH_BUF	256


/* put_chars_conv() */
void put_chars_conv(struct part *part, unsigned char *c, int l)
{
	static char buffer[CH_BUF];
	int bp = 0;
	int pp = 0;

	if (format.attr & AT_GRAPHICS) {
		put_chars(part, c, l);
		return;
	}

	if (!l) put_chars(part, NULL, 0);

	while (pp < l) {
		unsigned char *e;

		if (c[pp] < 128 && c[pp] != '&') {
putc:
			buffer[bp++] = c[pp++];
			if (bp < CH_BUF) continue;
			goto flush;
		}

		if (c[pp] != '&') {
			struct conv_table *t;
			int i;

			if (!convert_table) goto putc;
			t = convert_table;
			i = pp;

decode:
			if (!t[c[i]].t) {
				e = t[c[i]].u.str;
			} else {
				t = t[c[i++]].u.tbl;
				if (i >= l) goto putc;
				goto decode;
			}
			pp = i + 1;
		} else {
			int i = pp + 1;

			if (d_opt->plain) goto putc;
			while (i < l && c[i] != ';' && c[i] != '&' && c[i] > ' ') i++;
			e = get_entity_string(&c[pp + 1], i - pp - 1, d_opt->cp);
			if (!e) goto putc;
			pp = i + (i < l && c[i] == ';');
		}

		if (!e[0]) continue;

		if (!e[1]) {
			buffer[bp++] = e[0];
			if (bp < CH_BUF) continue;
flush:
			e = "";
			goto flush1;
		}

		while (*e) {
			buffer[bp++] = *(e++);
			if (bp < CH_BUF) continue;
flush1:
			put_chars(part, buffer, bp);
			bp = 0;
		}
	}
	if (bp) put_chars(part, buffer, bp);
}

#undef CH_BUF


/* put_chars() */
void put_chars(struct part *part, unsigned char *c, int l)
{
	static struct text_attrib_beginning ta_cache =
		{-1, {0, 0, 0}, {0, 0, 0}};
	static int bg_cache;
	static int fg_cache;
	int bg, fg;
	int i;
	struct link *link;
	struct point *pt;

	while (par_format.align != AL_NO && part->cx == -1 && l && *c == ' ') {
		c++;
		l--;
	}

	if (!l) return;

	if (c[0] != ' ' || (c[1] && c[1] != ' ')) {
		last_tag_for_newline = (void *)&part->data->tags;
	}
	if (part->cx == -1) part->cx = par_format.leftmargin;

	if (last_link || last_image || last_form || format.link
	    || format.image || format.form)
		goto process_link;

no_l:
	if (memcmp(&ta_cache, &format, sizeof(struct text_attrib_beginning)))
		goto format_change;
	bg = bg_cache, fg = fg_cache;

end_format_change:
	if (part->cx == par_format.leftmargin && *c == ' ' && par_format.align != AL_NO) {
		c++;
		l--;
	}
	if (part->y < part->cy + 1)
		part->y = part->cy + 1;

	set_hline(part, part->cx, part->cy, l, c, (((fg&0x08)<<3)|(bg<<3)|(fg&0x07))<<8, 1);
	part->cx += l;
	nobreak = 0;

	if (par_format.align != AL_NO) {
		while (part->cx > overlap(par_format) && part->cx > par_format.leftmargin) {
			int x;

#if 0
			if (part->cx > part->x) {
				part->x = part->cx + par_format.rightmargin;
				if (c[l - 1] == ' ') part->x--;
			}
#endif
			if (!(x = split_line(part))) break;

			/* if (LEN(part->cy-1) > part->x) part->x = LEN(part->cy-1); */

			align_line(part, part->cy - 1, 0);
			nobreak = x - 1;
		}
	}
	
#define TMP	part->xa - (c[l-1] == ' ' && par_format.align != AL_NO) + par_format.leftmargin + par_format.rightmargin
	part->xa += l;
	if (TMP > part->xmax) part->xmax = TMP;
#undef TMP

	return;

process_link:
	if ((last_link /*|| last_target*/ || last_image || last_form)
	    && !xstrcmp(format.link, last_link)
	    && !xstrcmp(format.target, last_target)
	    && !xstrcmp(format.image, last_image)
	    && format.form == last_form) {
		if (!part->data) goto x;
		link = &part->data->links[part->data->nlinks - 1];
		if (!part->data->nlinks) {
			internal("no link");
			goto no_l;
		}
		goto set_link;
x:;
	} else {
		if (last_link) mem_free(last_link);	/* !!! FIXME: optimize */
		if (last_target) mem_free(last_target);
		if (last_image) mem_free(last_image);

		last_link = last_target = last_image = NULL;
		last_form = NULL;

		if (!(format.link || format.image || format.form)) goto no_l;

		if (d_opt->num_links) {
			unsigned char s[64];
			unsigned char *fl = format.link;
			unsigned char *ft = format.target;
			unsigned char *fi = format.image;
			struct form_control *ff = format.form;

			format.link = format.target = format.image = NULL;
			format.form = NULL;
			s[0] = '[';
			snzprint(s + 1, 62, part->link_num);
			strcat(s, "]");

			put_chars(part, s, strlen(s));

			if (ff && ff->type == FC_TEXTAREA) line_break(part);
			if (part->cx == -1) part->cx = par_format.leftmargin;
			format.link = fl;
		   	format.target = ft;
			format.image = fi;
			format.form = ff;
		}

		part->link_num++;
		last_link = stracpy(format.link);
		last_target = stracpy(format.target);
		last_image = stracpy(format.image);
		last_form = format.form;

		if (!part->data) goto no_l;

		link = new_link(part->data);
		if (!link) goto no_l;

		link->num = format.tabindex + part->link_num - 1;
		link->accesskey = format.accesskey;
		link->pos = DUMMY;

		if (!last_form) {
			link->type = L_LINK;
			link->where = stracpy(last_link);
			link->target = stracpy(last_target);
		} else {
			switch (last_form->type) {
				case FC_TEXT:
				case FC_PASSWORD:
				case FC_FILE:
							link->type = L_FIELD;
							break;
				case FC_TEXTAREA:
							link->type = L_AREA;
							break;
				case FC_CHECKBOX:
				case FC_RADIO:
							link->type = L_CHECKBOX;
							break;
				case FC_SELECT:
							link->type = L_SELECT;
							break;
				default:
							link->type = L_BUTTON;
			}
			link->form = last_form;
			link->target = stracpy(last_form->target);
		}

		link->where_img = stracpy(last_image);

		if (link->type != L_FIELD && link->type != L_AREA) {
			bg = find_nearest_color(&format.clink, 8);
			fg = find_nearest_color(&format.bg, 8);
			fg = fg_color(fg, bg);
		} else {
			fg = find_nearest_color(&format.fg, 8);
			bg = find_nearest_color(&format.bg, 8);
			fg = fg_color(fg, bg);
		}

		link->sel_color = ((fg & 8) << 3) | (fg & 7) | (bg << 3);
		link->n = 0;
set_link:
		pt = mem_realloc(link->pos, (link->n + l) * sizeof(struct point));
		if (pt) {
			link->pos = pt;
			for (i = 0; i < l; i++) {
				pt[link->n + i].x = X(part->cx) + i;
				pt[link->n + i].y = Y(part->cy);
			}
			link->n += i;
		}
	}
	goto no_l;

format_change:
	bg = find_nearest_color(&format.bg, 8);
	fg = find_nearest_color(&format.fg, 16);
	fg = fg_color(fg, bg);

	if (format.attr & AT_ITALIC) fg = fg ^ 0x01;
	if (format.attr & AT_UNDERLINE) fg = (fg ^ 0x04) | 0x08;
	if (format.attr & AT_BOLD) fg = fg | 0x08;

	fg = fg_color(fg, bg);
	if (format.attr & AT_GRAPHICS) bg = bg | 0x10;

	memcpy(&ta_cache, &format, sizeof(struct text_attrib_beginning));
	fg_cache = fg;
	bg_cache = bg;
	goto end_format_change;
}

#undef overlap


/* line_break() */
void line_break(struct part *part)
{
	struct tag *t;

	if (part->cx + par_format.rightmargin > part->x)
		part->x = part->cx + par_format.rightmargin;
	if (nobreak) {
		/* if (part->y < part->cy) part->y = part->cy; */
		nobreak = 0;
		part->cx = -1;
		part->xa = 0;
		return;
	}

	if (!part->data) goto end;
	/* move_links(part, part->cx, part->cy, 0, part->cy + 1); */
	xpand_lines(part, part->cy + 1);
	if (part->cx > par_format.leftmargin
		&& (POS(part->cx-1, part->cy) & 0xff) == ' ') {
			del_chars(part, part->cx-1, part->cy);
		   	part->cx--;
	}

	/*if (LEN(part->cy) > part->x) part->x = LEN(part->cy);*/
	if (part->cx > 0) align_line(part, part->cy, 1);

	if (part->data) {
		for (t = last_tag_for_newline;
		     t && (void *)t != &part->data->tags;
		     t = t->prev) {
			t->x = X(0);
			t->y = Y(part->cy + 1);
		}
	}
end:
	part->cy++;
	part->cx = -1;
	part->xa = 0;
   	/* if (part->y < part->cy) part->y = part->cy; */
	memset(part->spaces, 0, part->spl);
}


/* html_init() */
void html_init(struct part *part)
{
	/* !!! FIXME: background */
}


/* destroy_fc() */
void destroy_fc(struct form_control *fc)
{
	int i;

	if (fc->action) mem_free(fc->action);
	if (fc->target) mem_free(fc->target);
	if (fc->name) mem_free(fc->name);
	if (fc->alt) mem_free(fc->alt);
	if (fc->default_value) mem_free(fc->default_value);

	for (i = 0; i < fc->nvalues; i++) {
		if (fc->values[i]) mem_free(fc->values[i]);
		if (fc->labels[i]) mem_free(fc->labels[i]);
	}

	if (fc->values) mem_free(fc->values);
	if (fc->labels) mem_free(fc->labels);
	if (fc->menu) free_menu(fc->menu);
}


/* html_form_control() */
void html_form_control(struct part *part, struct form_control *fc)
{
	if (!part->data) {
#if 0
		destroy_fc(fc);
		mem_free(fc);
#endif
		add_to_list(part->uf, fc);
		return;
	}

	fc->g_ctrl_num = g_ctrl_num++;

	/* if (fc->type == FC_TEXT || fc->type == FC_PASSWORD || fc->type == FC_TEXTAREA) */
	{
		int i;
		unsigned char *dv = convert_string(convert_table,
						   fc->default_value,
						   strlen(fc->default_value));

		if (dv) {
			mem_free(fc->default_value);
			fc->default_value = dv;
		}

		for (i = 0; i < fc->nvalues; i++) {
			dv = convert_string(convert_table, fc->values[i], strlen(fc->values[i]));
			if (dv) {
				mem_free(fc->values[i]);
				fc->values[i] = dv;
			}
		}
	}

	add_to_list(part->data->forms, fc);
}


/* add_frameset_entry() */
void add_frameset_entry(struct frameset_desc *fsd,
			struct frameset_desc *subframe,
			unsigned char *name, unsigned char *url)
{
	if (fsd->yp >= fsd->y) return;
#define TMP fsd->f[fsd->xp + fsd->yp * fsd->x]
	TMP.subframe = subframe;
	TMP.name = stracpy(name);
	TMP.url = stracpy(url);
#undef TMP
	fsd->xp++;
	if (fsd->xp >= fsd->x) {
		fsd->xp = 0;
		fsd->yp++;
	}
}


/* create_frameset() */
struct frameset_desc *create_frameset(struct f_data *fda, struct frameset_param *fp)
{
	int i;
	struct frameset_desc *fd;

	if (!fp->x || !fp->y) {
		internal("zero size of frameset");
		return NULL;
	}

#define TMP sizeof(struct frameset_desc) + fp->x * fp->y * sizeof(struct frame_desc)
	fd = mem_alloc(TMP);
	if (!fd) return NULL;
	memset(fd, 0, TMP);
#undef TMP

	fd->n = fp->x * fp->y;
	fd->x = fp->x;
	fd->y = fp->y;

	for (i = 0; i < fd->n; i++) {
		fd->f[i].xw = fp->xw[i % fp->x];
		fd->f[i].yw = fp->yw[i / fp->x];
	}

	if (fp->parent) add_frameset_entry(fp->parent, fd, NULL, NULL);
	else if (!fda->frame_desc) fda->frame_desc = fd;
	     else mem_free(fd), fd = NULL;

	return fd;
}


/* create_frame() */
void create_frame(struct frame_param *fp)
{
	add_frameset_entry(fp->parent, NULL, fp->name, fp->url);
}


/* html_special() */
void * html_special(struct part *part, enum html_special_type c, ...)
{
	va_list l;
	unsigned char *t;
	struct form_control *fc;
	struct frameset_param *fsp;
	struct frame_param *fp;

	va_start(l, c);
	switch (c) {
		case SP_TAG:
			t = va_arg(l, unsigned char *);
			html_tag(part->data, t, X(part->cx), Y(part->cy));
			break;
		case SP_CONTROL:
			fc = va_arg(l, struct form_control *);
			html_form_control(part, fc);
			break;
		case SP_TABLE:
			return convert_table;
		case SP_USED:
			return (void *)!!part->data;
		case SP_FRAMESET:
			fsp = va_arg(l, struct frameset_param *);
			return create_frameset(part->data, fsp);
		case SP_FRAME:
			fp = va_arg(l, struct frame_param *);
			create_frame(fp);
			break;
		default:
			internal("html_special: unknown code %d", c);
	}

	return NULL;
}


/* do_format() */
void do_format(char *start, char *end, struct part *part, unsigned char *head)
{
	parse_html(start, end,
		   (void (*)(void *, unsigned char *, int)) put_chars_conv,
		   (void (*)(void *)) line_break,
		   (void (*)(void *)) html_init,
		   (void *(*)(void *, int, ...)) html_special,
		   part, head);
	/* if ((part->y -= line_breax) < 0) part->y = 0; */
}

/* free_table_cache() */
void free_table_cache()
{
	free_list(table_cache);
}


/* format_html_part() */
struct part *format_html_part(unsigned char *start, unsigned char *end,
			      int align, int m, int width, struct f_data *data,
			      int xs, int ys, unsigned char *head,
			      int link_num)
{
	struct part *part;
	struct html_element *e;
	int llm = last_link_to_move;
	struct tag *ltm = last_tag_to_move;
	/*struct tag *ltn = last_tag_for_newline;*/
	int lm = margin;
	int ef = empty_format;
	struct form_control *fc;
	struct table_cache_entry *tce;

	if (!data) {
		foreach(tce, table_cache) {
			if (tce->start == start && tce->end == end
			    && tce->align == align && tce->m == m
			    && tce->width == width && tce->xs == xs
			    && tce->link_num == link_num) {
				part = mem_alloc(sizeof(struct part));
				if (!part) continue;
				memcpy(part, &tce->part, sizeof(struct part));
				return part;
			}
		}
	}

	if (ys < 0) {
		internal("format_html_part: ys == %d", ys);
		return NULL;
	}

	if (data) {
		struct node *n = mem_alloc(sizeof(struct node));

		if (n) {
			n->x = xs;
			n->y = ys;
			n->xw = !table_level ? MAXINT : width;
			add_to_list(data->nodes, n);
		}
		/*sdbg(data);*/
	}

	last_link_to_move = data ? data->nlinks : 0;
	last_tag_to_move = data ? (void *)&data->tags : NULL;
	last_tag_for_newline = data ? (void *)&data->tags: NULL;
	margin = m;
	empty_format = !data;

	if (last_link) mem_free(last_link);
	if (last_image) mem_free(last_image);
	if (last_target) mem_free(last_target);

	last_link = NULL;
	last_image = NULL;
	last_target = NULL;
	last_form = NULL;
	nobreak = 1;

	part = mem_alloc(sizeof(struct part));
	if (!part) goto ret;

	part->x = part->y = 0;
	part->data = data;
	part->xp = xs; part->yp = ys;
	part->xmax = part->xa = 0;
	part->bgcolor = find_nearest_color(&par_format.bgcolor, 8);
	part->spaces = DUMMY;
	part->spl = 0;
	part->link_num = link_num;

	init_list(part->uf);
	html_stack_dup();

	e = &html_top;
	html_top.dontkill = 2;
	html_top.namelen = 0;

	par_format.align = align;
	par_format.leftmargin = m;
	par_format.rightmargin = m;
	par_format.width = width;
	par_format.list_level = 0;
	par_format.list_number = 0;
	par_format.dd_margin = 0;
	part->cx = -1;
	part->cy = 0;

	do_format(start, end, part, head);

	if (part->xmax < part->x) part->xmax = part->x;

	nobreak = 0;
	line_breax = 1;

	if (last_link) mem_free(last_link);
	if (last_image) mem_free(last_image);
	if (last_target) mem_free(last_target);

	while (&html_top != e) {
		kill_html_stack_item(&html_top);
		if (!&html_top || (void *)&html_top == (void *)&html_stack) {
			internal("html stack trashed");
			break;
		}
	}

	html_top.dontkill = 0;
	kill_html_stack_item(&html_top);

	mem_free(part->spaces);

	if (data) {
		struct node *n = data->nodes.next;

		n->yw = ys - n->y + part->y;
	}

	foreach(fc, part->uf) destroy_fc(fc);
	free_list(part->uf);

ret:
	last_link_to_move = llm;
	last_tag_to_move = ltm;
	/*last_tag_for_newline = ltn;*/
	margin = lm;
	empty_format = ef;

	if (table_level > 1 && !data) {
		tce = mem_alloc(sizeof(struct table_cache_entry));
		if (tce) {
			tce->start = start;
			tce->end = end;
			tce->align = align;
			tce->m = m;
			tce->width = width;
			tce->xs = xs;
			tce->link_num = link_num;
			memcpy(&tce->part, part, sizeof(struct part));
			add_to_list(table_cache, tce);
		}
	}

	last_link = last_image = last_target = NULL;
	last_form = NULL;

	return part;
}


/* push_base_format() */
void push_base_format(unsigned char *url, struct document_options *opt)
{
	struct html_element *e;

	if (html_stack.next != &html_stack) {
		internal("something on html stack");
		init_list(html_stack);
	}

	e = mem_alloc(sizeof(struct html_element));
	if (!e) return;
	memset(e, 0, sizeof(struct html_element));

	add_to_list(html_stack, e);

	format.attr = 0;
	format.fontsize = 3;
	format.link = format.target = format.image = format.select = NULL;
	format.form = NULL;

	memcpy(&format.fg, &opt->default_fg, sizeof(struct rgb));
	memcpy(&format.bg, &opt->default_bg, sizeof(struct rgb));
	memcpy(&format.clink, &opt->default_link, sizeof(struct rgb));
	memcpy(&format.vlink, &opt->default_vlink, sizeof(struct rgb));

	format.href_base = stracpy(url);
	format.target_base = stracpy(opt->framename);

	par_format.align = opt->plain ? AL_NO : AL_LEFT;
	par_format.leftmargin = opt->plain ? 0 : opt->margin;
	par_format.rightmargin = opt->plain ? 0 : opt->margin;
	par_format.width = opt->xw;
	par_format.list_level = par_format.list_number = 0;
	par_format.dd_margin = opt->margin;
	par_format.flags = P_NONE;

	memcpy(&par_format.bgcolor, &opt->default_bg, sizeof(struct rgb));

	html_top.invisible = 0;
	html_top.name = NULL;
   	html_top.namelen = 0;
	html_top.options = NULL;
	html_top.linebreak = 1;
	html_top.dontkill = 1;
}


/* get_convert_table() */
struct conv_table *get_convert_table(unsigned char *head, int to,
				     int def, int *frm, int *aa, int hard)
{
	int from = -1;
	unsigned char *a, *b;
	unsigned char *part = head;

	while (from == -1 && part) {
		a = parse_http_header(part, "Content-Type", &part);
		if (a) {
			b = parse_http_header_param(a, "charset");
			if (b) {
				from = get_cp_index(b);
				mem_free(b);
			}
			mem_free(a);
		} else break;
	}

	if (from == -1 && head) {
		a = parse_http_header(head, "Content-Charset", NULL);
		if (a) {
			from = get_cp_index(a);
			mem_free(a);
		}
	}

	if (from == -1 && head) {
		a = parse_http_header(head, "Charset", NULL);
		if (a) {
			from = get_cp_index(a);
			mem_free(a);
		}
	}

	if (aa) {
		*aa = (from == -1);
		if (hard && !*aa) *aa = 2;
	}

	if (hard || from == -1) from = def;
	if (frm) *frm = from;

	return get_translation_table(from, to);
}


/* format_html() */
void format_html(struct cache_entry *ce, struct f_data *screen)
{
	unsigned char *url = ce->url;
	struct fragment *fr;
	struct part *rp;
	unsigned char *start = NULL;
	unsigned char *end = NULL;
	unsigned char *head, *t;
	int hdl;
	int i;

	d_opt = &screen->opt;
	screen->use_tag = ce->count;
	defrag_entry(ce);
	fr = ce->frag.next;

	if (!((void *)fr == &ce->frag || fr->offset || !fr->length)) {
		start = fr->data;
		end = fr->data + fr->length;
	}

	startf = start;
	eofff = end;

	head = init_str();
	hdl = 0;
	if (ce->head) add_to_str(&head, &hdl, ce->head);

	scan_http_equiv(start, end, &head, &hdl, &t);

	convert_table = get_convert_table(head, screen->opt.cp,
					  screen->opt.assume_cp,
					  &screen->cp, &screen->ass,
					  screen->opt.hard_assume);

	i = d_opt->plain;
	d_opt->plain = 0;

	screen->title = convert_string(convert_table, t, strlen(t));
	d_opt->plain = i;

	mem_free(t);

	push_base_format(url, &screen->opt);

	table_level = 0;
	g_ctrl_num = 0;
	last_form_tag = NULL;
	last_form_attr = NULL;
	last_input_tag = NULL;

	rp = format_html_part(start, end, par_format.align,
			      par_format.leftmargin, screen->opt.xw, screen,
			      0, 0, head, 1);
	if (rp) mem_free(rp);

	mem_free(head);

	screen->x = 0;

	for (i = screen->y - 1; i >= 0; i--) {
		if (!screen->data[i].l) {
			mem_free(screen->data[i].d);
			screen->y--;
		} else break;
	}

	for (i = 0; i < screen->y; i++)
		if (screen->data[i].l > screen->x)
			screen->x = screen->data[i].l;

	if (form.action) {
		mem_free(form.action);
	   	form.action = NULL;
	}

	if (form.target) {
		mem_free(form.target);
		form.target = NULL;
	}

	kill_html_stack_item(html_stack.next);

	if (html_stack.next != &html_stack) {
		internal("html stack not empty after operation");
		init_list(html_stack);
	}

	screen->bg = 007 << 8; /* !!! FIXME */
	sort_links(screen);

	if (screen->frame_desc) screen->frame = 1;

#if 0
		FILE *f = fopen("forms", "a");
		struct form_control *form;
		unsigned char *qq;
		fprintf(f,"FORM:\n");
		foreach(form, screen->forms) {
			fprintf(f, "g=%d f=%d c=%d t:%d\n",
				form->g_ctrl_num, form->form_num,
				form->ctrl_num, form->type);
		}
		fprintf(f,"fragment: \n");
		for (qq = start; qq < end; qq++) fprintf(f, "%c", *qq);
		fprintf(f,"----------\n\n");
		fclose(f);
#endif
}


/* shrink_format_cache() */
void shrink_format_cache(int u)
{
	struct f_data *ce;

	delete_unused_format_cache_entries();

	if (format_cache_entries < 0) {
		internal("format_cache_entries underflow");
		format_cache_entries = 0;
	}

	ce = format_cache.prev;
	while ((u || format_cache_entries > max_format_cache_entries)
	       && (void *)ce != &format_cache) {

		if (ce->refcount) {
			ce = ce->prev;
			continue;
		}

		ce = ce->prev;
		destroy_formatted(ce->next);
		format_cache_entries--;
	}
}


/* count_format_cache() */
void count_format_cache()
{
	struct f_data *ce;

	format_cache_entries = 0;
	foreach(ce, format_cache) if (!ce->refcount) format_cache_entries++;
}


/* delete_unused_format_cache_entries() */
void delete_unused_format_cache_entries()
{
	struct f_data *ce;

	foreach(ce, format_cache) {
		struct cache_entry *cee = NULL;

		if (!ce->refcount) {
			if (find_in_cache(ce->url, &cee) || !cee
			    || cee->count != ce->use_tag) {
				if (!cee) internal("file %s disappeared from cache", ce->url);
				ce = ce->prev;
				destroy_formatted(ce->next);
				format_cache_entries--;
			}
		}
	}
}


/* format_cache_reactivate */
void format_cache_reactivate(struct f_data *ce)
{
	del_from_list(ce);
	add_to_list(format_cache, ce);
}


/* cached_format_html() */
void cached_format_html(struct view_state *vs, struct f_data_c *screen,
						struct document_options *opt)
{
	unsigned char *n;
	struct f_data *ce;
	struct cache_entry *cee;

	if (!vs) return;

	n = screen->name;
	screen->name = NULL;
	detach_formatted(screen);

	screen->name = n;
	screen->link_bg = NULL;
	screen->link_bg_n = 0;
	if (vs) vs->f = screen;

	screen->vs = vs;
	screen->xl = screen->yl = -1;
	screen->f_data = NULL;

	foreach(ce, format_cache) {
		if (strcmp(ce->url, vs->url)) continue;
		if (compare_opt(&ce->opt, opt)) continue;
		
		cee = NULL;
		if (find_in_cache(vs->url, &cee) || !cee || cee->count != ce->use_tag) {
			if (!cee) internal("file %s disappeared from cache", ce->url);
			if (!ce->refcount) {
				ce = ce->prev;
				destroy_formatted(ce->next);
				format_cache_entries--;
			}
			continue;
		}

		format_cache_reactivate(ce);

		if (!ce->refcount++) format_cache_entries--;
		screen->f_data = ce;

		goto sx;
	}

	if (find_in_cache(vs->url, &cee) || !cee) {
		internal("document to format not found");
		return;
	}

	cee->refcount++;
	shrink_memory(0);

	ce = mem_alloc(sizeof(struct f_data));
	if (!ce) {
		cee->refcount--;
		return;
	}

	init_formatted(ce);
	ce->refcount = 1;

	ce->url = mem_alloc(strlen(vs->url) + 1);
	if (!ce->url) {
		mem_free(ce);
		cee->refcount--;
		return;
	}

	strcpy(ce->url, vs->url);
	copy_opt(&ce->opt, opt);
	add_to_list(format_cache, ce);

	screen->f_data = ce;
	ce->time_to_get = -get_time();
	format_html(cee, ce);
	ce->time_to_get += get_time();

sx:
	screen->xw = ce->opt.xw;
	screen->yw = ce->opt.yw;
	screen->xp = ce->opt.xp;
	screen->yp = ce->opt.yp;
}


/* formatted_info() */
long formatted_info(int type)
{
	int i = 0;
	struct f_data *ce;

	switch (type) {
		case CI_FILES:
			foreach(ce, format_cache) i++;
			return i;
		case CI_LOCKED:
			foreach(ce, format_cache) i += !!ce->refcount;
			return i;
		default:
			internal("formatted_info: bad request");
	}

	return 0;
}


/* add_frame_to_list() */
void add_frame_to_list(struct session *ses, struct f_data_c *fd)
{
	struct f_data_c *f;

	foreach(f, ses->scrn_frames) {
		if (f->yp > fd->yp || (f->yp == fd->yp && f->xp > fd->xp)) {
			add_at_pos(f->prev, fd);
			return;
		}
	}

	add_at_pos((struct f_data_c *)ses->scrn_frames.prev, fd);
}


/* find_fd() */
struct f_data_c *find_fd(struct session *ses, unsigned char *name,
			 int depth, int x, int y)
{
	struct f_data_c *fd;

	foreachback(fd, ses->scrn_frames) {
		if (!strcasecmp(fd->name, name) && !fd->used) {
			fd->used = 1;
			fd->depth = depth;
			return fd;
		}
	}

	fd = mem_alloc(sizeof(struct f_data_c));
	if (!fd) return NULL;
	memset(fd, 0, sizeof(struct f_data_c));

	fd->used = 1;
	fd->name = stracpy(name);
	fd->depth = depth;
	fd->xp = x, fd->yp = y;
	fd->search_word = &ses->search_word;

	/*add_to_list(ses->scrn_frames, fd);*/
	add_frame_to_list(ses, fd);

	return fd;
}


/* format_frame() */
struct f_data_c *format_frame(struct session *ses, unsigned char *name,
			      unsigned char *url, struct document_options *o,
			      int depth)
{
	struct cache_entry *ce;
	struct view_state *vs;
	struct f_data_c *fd;
	struct frame *fr;

repeat:
	fr = ses_find_frame(ses, name);
	if (!fr) return NULL;

	vs = &fr->vs;
	if (find_in_cache(vs->url, &ce) || !ce) return NULL;

	if (ce->redirect && fr->redirect_cnt < MAX_REDIRECTS) {
		unsigned char *u = join_urls(vs->url, ce->redirect);

		if (u) {
			fr->redirect_cnt++;
			ses_change_frame_url(ses, name, u);
			mem_free(u);
			goto repeat;
		}
	}

	fd = find_fd(ses, name, depth, o->xp, o->yp);
	if (!fd) return NULL;

	cached_format_html(vs, fd, o);
	return fd;
}


/* format_frames() */
void format_frames(struct session *ses, struct frameset_desc *fsd,
		   struct document_options *op, int depth)
{
	int i, j, n;
	struct document_options o;

	if (depth > HTML_MAX_FRAME_DEPTH) return;

	memcpy(&o, op, sizeof(struct document_options));

	if (o.margin) o.margin = 1;

	n = 0;
	for (j = 0; j < fsd->y; j++) {
		o.xp = op->xp;
		for (i = 0; i < fsd->x; i++) {
			struct f_data_c *fdc;

			o.xw = fsd->f[n].xw;
			o.yw = fsd->f[n].yw;
			o.framename = fsd->f[n].name;
			if (fsd->f[n].subframe)
				format_frames(ses, fsd->f[n].subframe, &o, depth + 1);
			else if (fsd->f[n].name) {
				fdc = format_frame(ses, fsd->f[n].name, fsd->f[n].url, &o, depth);
				if (fdc && fdc->f_data && fdc->f_data->frame)
					format_frames(ses, fdc->f_data->frame_desc, &o, depth + 1);
			}
			o.xp += o.xw + 1;
			n++;
		}
		o.yp += o.yw + 1;
	}

#if 0
	for (i = 0; i < fsd->n; i++) {
		if (!fsd->horiz) o.xw = fsd->f[i].width;
		else o.yw = fsd->f[i].width;
		o.framename = fsd->f[i].name;
		if (fsd->f[i].subframe) format_frames(ses, fsd->f[i].subframe, &o);
		else format_frame(ses, fsd->f[i].name, fsd->f[i].url, &o);
		if (!fsd->horiz) o.xp += fsd->f[i].width + 1;
		else o.yp += fsd->f[i].width + 1;
	}
#endif
}


/* html_interpret() */
void html_interpret(struct session *ses)
{
	struct document_options o;
	struct f_data_c *fd;
	struct f_data_c *cf = NULL;
	struct view_state *l = NULL;

	if (!ses->screen) {
		ses->screen = mem_alloc(sizeof(struct f_data_c));
		if (!ses->screen) return;
		memset(ses->screen, 0, sizeof(struct f_data_c));
		ses->screen->search_word = &ses->search_word;
	}

	if (!list_empty(ses->history)) l = &cur_loc(ses)->vs;

	o.xp = 0;
	o.yp = 1;
	o.xw = ses->term->x;
	o.yw = ses->term->y - 2;
	o.col = ses->term->spec->col;
	o.cp = ses->term->spec->charset;

	ds2do(&ses->ds, &o);

	/* FIXME: enigmatic code by Mikulas Corp. -- Zas*/
	if ((o.plain = l ? l->plain : 1) == -1) o.plain = 0;
	if (l) l->plain = o.plain;

	memcpy(&o.default_fg, &default_fg, sizeof(struct rgb));
	memcpy(&o.default_bg, &default_bg, sizeof(struct rgb));
	memcpy(&o.default_link, &default_link, sizeof(struct rgb));
	memcpy(&o.default_vlink, &default_vlink, sizeof(struct rgb));

	o.framename = "";

	foreach(fd, ses->scrn_frames) fd->used = 0;

	cached_format_html(l, ses->screen, &o);

	if (ses->screen->f_data && ses->screen->f_data->frame) {
		cf = current_frame(ses);
		format_frames(ses, ses->screen->f_data->frame_desc, &o, 0);
	}

	foreach(fd, ses->scrn_frames) if (!fd->used) {
		struct f_data_c *fdp = fd->prev;

		detach_formatted(fd);
		del_from_list(fd);
		mem_free(fd);
		fd = fdp;
	}

	if (cf) {
		int n = 0;

		foreach(fd, ses->scrn_frames) {
			if (fd->f_data && fd->f_data->frame) continue;
			if (fd == cf) {
				cur_loc(ses)->vs.current_link = n;
				break;
			}
			n++;
		}
	}
}


#define SRCH_ALLOC_GR	0x10000


/* add_srch_chr() */
void add_srch_chr(struct f_data *f, unsigned char c, int x, int y, int nn)
{
	int n = f->nsearch;

	if (c == ' ' && (!n || f->search[n - 1].c == ' ')) return;
	f->search[n].c = c;
	f->search[n].x = x;
	f->search[n].y = y;
	f->search[n].n = nn;
	f->nsearch++;
}

#if 0
void sdbg(struct f_data *f)
{
	struct node *n;
	foreachback(n, f->nodes) {
		int xm = n->x + n->xw, ym = n->y + n->yw;
		printf("%d %d - %d %d\n", n->x, n->y, xm, ym);
		fflush(stdout);
	}
	debug("!");
}
#endif


/* sort_srch() */
void sort_srch(struct f_data *f)
{
	int i;
	int *min, *max;

	f->slines1 = mem_alloc(f->y * sizeof(struct search *));
	if (!f->slines1) return;

	f->slines2 = mem_alloc(f->y * sizeof(struct search *));
	if (!f->slines2) {
		mem_free(f->slines1);
		return;
	}

	min = mem_alloc(f->y * sizeof(int));
	if (!min) {
		mem_free(f->slines1);
		mem_free(f->slines2);
		return;
	}

	max = mem_alloc(f->y * sizeof(int));
	if (!max) {
		mem_free(f->slines1);
		mem_free(f->slines2);
		mem_free(min);
		return;
	}

	memset(f->slines1, 0, f->y * sizeof(struct search *));
	memset(f->slines2, 0, f->y * sizeof(struct search *));

	for (i = 0; i < f->y; i++) {
		min[i] = MAXINT;
		max[i] = 0;
	}

	for (i = 0; i < f->nsearch; i++) {
		struct search *s = &f->search[i];

		if (s->x < min[s->y]) {
			min[s->y] = s->x;
		   	f->slines1[s->y] = s;
		}
		if (s->x + s->n > max[s->y]) {
			max[s->y] = s->x + s->n;
			f->slines2[s->y] = s;
		}
	}

	mem_free(min);
	mem_free(max);
}



/* get_srch() */
int get_srch(struct f_data *f)
{
	struct node *n;
	int cnt = 0;
	int cc = !f->search;

	foreachback(n, f->nodes) {
		int x, y;
		int xm = n->x + n->xw;
		int ym = n->y + n->yw;

#if 0
		printf("%d %d - %d %d\n", n->x, n->y, xm, ym);
		fflush(stdout);
#endif

		for (y = n->y; y < ym && y < f->y; y++) {
			int ns = 1;

			for (x = n->x; x < xm && x < f->data[y].l; x++) {
				unsigned char c = f->data[y].d[x];

				if (c < ' ') c = ' ';
				if (c == ' ' && ns) continue;

				if (ns) {
					if (!cc) add_srch_chr(f, c, x, y, 1);
					else cnt++;
					ns = 0;
					continue;
				}

				if (c != ' ') {
					if (!cc) add_srch_chr(f, c, x, y, 1);
					else cnt++;
				} else {
					int xx;

					for (xx = x + 1; xx < xm && xx < f->data[y].l; xx++)
						if ((unsigned char) f->data[y].d[xx] >= ' ')
							goto cont;

					xx = x;

cont:
					if (!cc) add_srch_chr(f, ' ', x, y, xx - x);
					else cnt++;

					if (xx == x) break;
					x = xx - 1;
				}

			}

			if (!cc) add_srch_chr(f, ' ', x, y, 0);
			else cnt++;
		}

	}

	return cnt;
}


/* get_search_data() */
void get_search_data(struct f_data *f)
{
	int n;

	if (f->search) return;

	n = get_srch(f);
	f->nsearch = 0;

	f->search = mem_alloc(n * sizeof(struct search));
	if (!f->search) return;

	get_srch(f);
	while (f->nsearch && f->search[f->nsearch - 1].c == ' ') f->nsearch--;
	sort_srch(f);
}
