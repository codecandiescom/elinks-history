/* HTML renderer */
/* $Id: renderer.c,v 1.105 2003/06/16 14:45:01 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <stdarg.h>
#include <string.h>

#include "elinks.h"

#include "main.h"
#include "bfu/align.h"
#include "config/options.h"
#include "document/cache.h"
#include "document/options.h"
#include "document/html/colors.h"
#include "document/html/parser.h"
#include "document/html/renderer.h"
#include "document/html/tables.h"
#include "intl/charsets.h"
#include "lowlevel/ttime.h"
#include "protocol/http/header.h"
#include "protocol/url.h"
#include "sched/session.h"
#include "terminal/draw.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/hash.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/view.h"
#include "viewer/text/vs.h"

/* Types and structs */

struct table_cache_entry_key {
	unsigned char *start;
	unsigned char *end;
	int align;
	int m;
	int width;
	int xs;
	int link_num;
};

struct table_cache_entry {
	LIST_HEAD(struct table_cache_entry);

	struct table_cache_entry_key key;
	struct part part;
};

/* Max. entries in table cache used for nested tables. */
#define MAX_TABLE_CACHE_ENTRIES 16384

/* Global variables */
int margin;
int format_cache_entries = 0;

static int table_cache_entries = 0;
static int last_link_to_move;
static struct tag *last_tag_to_move;
static struct tag *last_tag_for_newline;
static unsigned char *last_link;
static unsigned char *last_target;
static unsigned char *last_image;
static struct form_control *last_form;
static int nobreak;
static struct conv_table *convert_table;
static int g_ctrl_num;
static int empty_format;

static struct hash *table_cache = NULL;
static INIT_LIST_HEAD(format_cache);


/* Prototypes */
void line_break(struct part *);
void put_chars(struct part *, unsigned char *, int);


#ifdef ALIGN
#undef ALIGN
#endif

#define ALIGN(x) (((x)+0x7f)&~0x7f)

static int nowrap = 0; /* Activated/deactivated by SP_NOWRAP. */
static int sub = 0; /* Activated/deactivated by AT_SUBSCRIPT */
static int super = 0; /* Activated/deactivated by AT_SUPERSCRIPT */


static int
realloc_lines(struct part *p, int y)
{
	int i;
	int newsize = ALIGN(y + 1);

	if (newsize >= ALIGN(p->data->y)
	    && (!p->data->data || p->data->data->size < newsize)) {
		struct line *l;

		l = mem_realloc(p->data->data, newsize * sizeof(struct line));
		if (!l)	return -1;

		p->data->data = l;
		p->data->data->size = newsize;
	}

	for (i = p->data->y; i <= y; i++) {
		p->data->data[i].l = 0;
		p->data->data[i].c = find_nearest_color(&par_format.bgcolor, 8);
		p->data->data[i].d = NULL;
	}

	p->data->y = i;

	return 0;
}

static int
realloc_line(struct part *p, int y, int x)
{
	int i;
	int newsize = ALIGN(x + 1);

	if (newsize >= ALIGN(p->data->data[y].l)
	    && (!p->data->data[y].d || p->data->data[y].dsize < newsize)) {
		chr *l;

		l = mem_realloc(p->data->data[y].d, newsize * sizeof(chr));
		if (!l)	return -1;

		p->data->data[y].d = l;
		p->data->data[y].dsize = newsize;
	}

	p->data->data[y].c = find_nearest_color(&par_format.bgcolor, 8);

	for (i = p->data->data[y].l; i <= x; i++) {
		p->data->data[y].d[i] = (p->data->data[y].c << 11) | ' ';
	}

	p->data->data[y].l = i;

	return 0;
}

#undef ALIGN

static inline int
xpand_lines(struct part *p, int y)
{
	/*if (y >= p->y) p->y = y + 1;*/
	if (!p->data) return 0;
	y += p->yp;
	if (y >= p->data->y) return realloc_lines(p, y);

	return 0;
}

int
expand_lines(struct part *part, int y)
{
	return xpand_lines(part, y);
}

static inline int
xpand_line(struct part *p, int y, int x)
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

int
expand_line(struct part *part, int y, int x)
{
	return xpand_line(part, y, x);
}

static inline int
realloc_spaces(struct part *p, int l)
{
	unsigned char *c;

	c = mem_realloc(p->spaces, l + 1);
	if (!c) return -1;
	memset(c + p->spaces_len, 0, l - p->spaces_len + 1);

	p->spaces_len = l + 1;
	p->spaces = c;

	return 0;
}

static inline int
xpand_spaces(struct part *p, int l)
{
	if (l >= p->spaces_len) return realloc_spaces(p, l);
	return 0;
}


#define POS(x, y) (part->data->data[part->yp + (y)].d[part->xp + (x)])
#define LEN(y) (part->data->data[part->yp + (y)].l - part->xp < 0 ? 0 : part->data->data[part->yp + (y)].l - part->xp)
#define SLEN(y, x) part->data->data[part->yp + (y)].l = part->xp + x;
#define X(x) (part->xp + (x))
#define Y(y) (part->yp + (y))


static inline void
set_hchar(struct part *part, int x, int y, unsigned c)
{
	if (xpand_lines(part, y)
	    || xpand_line(part, y, x))
		return;
	POS(x, y) = c;
}

static inline void
set_hchars(struct part *part, int x, int y, int xl, unsigned c)
{
	if (xpand_lines(part, y)
	    || xpand_line(part, y, x + xl - 1))
		return;
	for (; xl; xl--, x++) POS(x, y) = c;
}

void
xset_hchar(struct part *part, int x, int y, unsigned c)
{
	set_hchar(part, x, y, c);
}

void
xset_hchars(struct part *part, int x, int y, int xl, unsigned c)
{
	set_hchars(part, x, y, xl, c);
}

static inline void
set_hline(struct part *part, int x, int y,
	  unsigned char *chars, int charslen, unsigned attr)
{
	if (xpand_lines(part, y)
	    || xpand_line(part, y, x + charslen - 1)
	    || (xpand_spaces(part, x + charslen - 1)))
		return;

	for (; charslen > 0; charslen--, x++, chars++) {
		part->spaces[x] = (*chars == ' ');
		if (part->data) POS(x, y) = (*chars | attr);
	}
}

static void
move_links(struct part *part, int xf, int yf, int xt, int yt)
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

static inline void
copy_chars(struct part *part, int x, int y, int xl, chr *d)
{
	if (xl <= 0
	    || xpand_lines(part, y)
	    || xpand_line(part, y, x + xl - 1))
		return;
	for (; xl; xl--, x++, d++) POS(x, y) = *d;
}

static inline void
move_chars(struct part *part, int x, int y, int nx, int ny)
{
	if (LEN(y) - x <= 0) return;
	copy_chars(part, nx, ny, LEN(y) - x, &POS(x, y));
	SLEN(y, x);
	move_links(part, x, y, nx, ny);
}

static inline void
shift_chars(struct part *part, int y, int shift)
{
	chr *a;
	int len = LEN(y);

	a = fmem_alloc(len * sizeof(chr));
	if (!a) return;

	memcpy(a, &POS(0, y), len * sizeof(chr));
	/* XXX: This is fundamentally broken and it gives us those color stains
	 * all over spanning from the colorful table cells. We asume that the
	 * whole line is one-colored here, but we should take definitively more
	 * care. But this looks like a fundamental design flaw and it'd require
	 * us to rewrite big parts of code, I fear (but maybe I'm mistaken and
	 * you need to just turn one bit somewhere in the code!)... Note that
	 * using find_nearest_color(par_format.bgcolor, 8) doesn't work here, I
	 * already got that idea; results in even more stains since we probably
	 * shift chars even on surrounding lines when realigning tables
	 * maniacally. --pasky */
	set_hchars(part, 0, y, shift, (part->data->data[y].c << 11) | ' ');
	copy_chars(part, shift, y, len, a);
	fmem_free(a);

	move_links(part, 0, y, shift, y);
}

static inline void
del_chars(struct part *part, int x, int y)
{
	SLEN(y, x);
	move_links(part, x, y, -1, -1);
}

#define overlap(x) ((x).width - (x).rightmargin > 0 ? (x).width - (x).rightmargin : 0)
/* FIXME: Understand it and comment it ...
 * Previous code was kept in #if 0/#endif, see below. */
static int
split_line(struct part *part)
{
	register int i; /* What is i ? */
	register int tmp;

	for (i = overlap(par_format); i >= par_format.leftmargin; i--)
		if (i < part->spaces_len && part->spaces[i])
			goto split;

	for (i = par_format.leftmargin; i < part->cx ; i++)
		if (i < part->spaces_len && part->spaces[i])
			goto split;

	tmp = part->cx + par_format.rightmargin;
	if (tmp > part->x)
		part->x = tmp;

	return 0; /* XXX: What does this mean ? */

split:
	tmp = i + par_format.rightmargin;
	if (tmp > part->x)
		part->x = tmp;

	if (part->data) {
#ifdef DEBUG
		if ((POS(i, part->cy) & 0xff) != ' ')
			internal("bad split: %c", (char)POS(i, part->cy));
#endif
		move_chars(part, i + 1, part->cy, par_format.leftmargin, part->cy + 1);
		del_chars(part, i, part->cy);
	}

	i++; /* Since we were using (i + 1) only later... */

	tmp = part->spaces_len - i;
	if (tmp > 0) /* 0 is possible and i'm paranoiac ... --Zas */
		memmove(part->spaces, part->spaces + i, tmp);

	/* XXX: is this correct ??? tmp <= 0 case ? --Zas */
	memset(part->spaces + tmp, 0, i);

	tmp = part->spaces_len - par_format.leftmargin;
	if (tmp > 0)
		memmove(part->spaces + par_format.leftmargin, part->spaces, tmp);
	else	/* Should not occcur. --Zas */
		internal("part->spl - par_format.leftmargin == %d", tmp);

	/* Following should be equivalent to old code (see below)
	 * please verify and simplify if possible. */
	part->cy++;

	if (part->cx == i) {
		part->cx = -1;
		if (part->y < part->cy) part->y = part->cy;
		return 2; /* XXX: mean ? */
	} else {
		part->cx -= i - par_format.leftmargin;
		if (part->y < part->cy + 1) part->y = part->cy + 1;
		return 1; /* XXX: mean ? */

	}
}


#if 0
/* TODO: optimization and verification needed there. --Zas */
static int
split_line(struct part *part)
{
	int i;

#if 0
	if (!part->data) goto r;
	printf("split: %d,%d   , %d,%d,%d\n",part->cx,part->cy,par_format.rightmargin,par_format.leftmargin,part->cx);
#endif

	for (i = overlap(par_format); i >= par_format.leftmargin; i--)
		if (i < part->spaces_len && part->spaces[i])
			goto split;

#if 0
	for (i = part->cx - 1; i > overlap(par_format) && i > par_format.leftmargin; i--)
#endif

	for (i = par_format.leftmargin; i < part->cx ; i++)
		if (i < part->spaces_len && part->spaces[i])
			goto split;

#if 0
	for (i = overlap(par_format); i >= par_format.leftmargin; i--)
		if ((POS(i, part->cy) & 0xff) == ' ')
			goto split;
	for (i = part->cx - 1; i > overlap(par_format) && i > par_format.leftmargin; i--)
		if ((POS(i, part->cy) & 0xff) == ' ')
			goto split;
#endif

	if (part->cx + par_format.rightmargin > part->x)
		part->x = part->cx + par_format.rightmargin;

#if 0
	if (part->y < part->cy + 1) part->y = part->cy + 1;
	part->cy++; part->cx = -1;
	memset(part->spaces, 0, part->spaces_len);
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

	/* FYI, witekfl has an optimization of this pending in my patch queue.
	 * I want to review it since it's kinda arcane and error here would
	 * probably mean some quite interesting random bugs. --pasky */

	if (part->spaces_len - i - 1 > 0) {
		/* 0 is possible and i'm paranoic ... --Zas */
		memmove(part->spaces, part->spaces + i + 1,
			part->spaces_len - i - 1);
	}

	memset(part->spaces + part->spaces_len - i - 1, 0, i + 1);

	if (part->spaces_len - par_format.leftmargin > 0)
		memmove(part->spaces + par_format.leftmargin, part->spaces,
			part->spaces_len - par_format.leftmargin);
	else	/* Should not occcur. --Zas */
		internal("part->spaces_len - par_format.leftmargin == %d",
			 part->spaces_len - par_format.leftmargin);

	part->cx -= i - par_format.leftmargin + 1;
	part->cy++;

	/*return 1 + (part->cx == par_format.leftmargin);*/

	if (part->cx == par_format.leftmargin) part->cx = -1;
	if (part->y < part->cy + (part->cx != -1)) part->y = part->cy + (part->cx != -1);

	return 1 + (part->cx == -1);
}
#endif

/* This function is very rare exemplary of clean and beautyful code here.
 * Please handle with care. --pasky */
static void
justify_line(struct part *part, int y)
{
	chr *line; /* we save original line here */
	int len = LEN(y);
	int pos;
	int *space_list;
	int spaces;

	line = fmem_alloc(len * sizeof(chr));
	if (!line) return;

	/* It may sometimes happen that the line is only one char long and that
	 * char is space - then we're going to write to both [0] and [1], but
	 * we allocated only one field. Thus, we've to do (len + 1). --pasky */
	space_list = fmem_alloc((len + 1) * sizeof(int));
	if (!space_list) {
		fmem_free(line);
		return;
	}

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

		/* See shift_chars() about why this is broken. */
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

	fmem_free(space_list);
	fmem_free(line);
}

static void
align_line(struct part *part, int y, int last)
{
	int shift;
	int len;

	if (!part->data)
		return;

	len = LEN(y);

	if (!len || par_format.align == AL_LEFT ||
		    par_format.align == AL_NONE)
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

static struct link *
new_link(struct f_data *f)
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

static void
html_tag(struct f_data *f, unsigned char *t, int x, int y)
{
	struct tag *tag;
	int tsize;

	if (!f) return;

	tsize = strlen(t) + 1;
	tag = mem_alloc(sizeof(struct tag) + tsize);
	if (tag) {
		tag->x = x;
		tag->y = y;
		memcpy(tag->name, t, tsize);
		add_to_list(f->tags, tag);
		if ((void *) last_tag_for_newline == &f->tags)
			last_tag_for_newline = tag;
	}
}


#define CH_BUF	256

static void
put_chars_conv(struct part *part, unsigned char *chars, int charslen)
{
	static char buffer[CH_BUF];
	int bp = 0;
	int pp = 0;

	if (format.attr & AT_GRAPHICS) {
		put_chars(part, chars, charslen);
		return;
	}

	if (!charslen) put_chars(part, NULL, 0);

	/* FIXME: Code redundancy with convert_string() in charsets.c. --Zas */
	while (pp < charslen) {
		unsigned char *e;

		if (chars[pp] < 128 && chars[pp] != '&') {
putc:
			buffer[bp++] = chars[pp++];
			if (bp < CH_BUF) continue;
			goto flush;
		}

		if (chars[pp] != '&') {
			struct conv_table *t;
			int i;

			if (!convert_table) goto putc;
			t = convert_table;
			i = pp;

decode:
			if (!t[chars[i]].t) {
				e = t[chars[i]].u.str;
			} else {
				t = t[chars[i++]].u.tbl;
				if (i >= charslen) goto putc;
				goto decode;
			}
			pp = i + 1;
		} else {
			int start = pp + 1;
			int i = start;

			if (d_opt->plain) goto putc;
			while (i < charslen
			       && ((chars[i] >= 'A' && chars[i] <= 'Z')
				   || (chars[i] >= 'a' && chars[i] <= 'z')
				   || (chars[i] >= '0' && chars[i] <= '9')
				   || (chars[i] == '#')))
				i++;

			/* Eat &nbsp &nbsp<foo>. --Zas ;) */
			if (!isalnum(chars[i]) && i > start) {
				e = get_entity_string(&chars[start], i - start,
						d_opt->cp);
				if (!e) goto putc;
				pp = i + (i < charslen);
			} else goto putc;
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

void
put_chars(struct part *part, unsigned char *chars, int charslen)
{
	static struct text_attrib_beginning ta_cache =
		{-1, {0, 0, 0}, {0, 0, 0}};
	static int bg_cache;
	static int fg_cache;
	int bg, fg;
	int i;
	struct link *link;
	struct point *pt;
	int tmp; /* used for temporary results. */

	while (par_format.align != AL_NONE && part->cx == -1
	       && charslen && *chars == ' ') {
		chars++;
		charslen--;
	}

	if (!charslen) return;

	if (chars[0] != ' ' || (chars[1] && chars[1] != ' ')) {
		last_tag_for_newline = (void *)&part->data->tags;
	}
	if (part->cx == -1) part->cx = par_format.leftmargin;

	if (last_link || last_image || last_form || format.link
	    || format.image || format.form)
		goto process_link;

no_l:
	if (memcmp(&ta_cache, &format, sizeof(struct text_attrib_beginning)))
		goto format_change;
	bg = bg_cache;
	fg = fg_cache;

end_format_change:
	if (part->cx == par_format.leftmargin && *chars == ' '
	    && par_format.align != AL_NONE) {
		chars++;
		charslen--;
	}
	if (part->y < part->cy + 1)
		part->y = part->cy + 1;

	if (nowrap && part->cx + charslen > overlap(par_format))
		return;

	set_hline(part, part->cx, part->cy, chars, charslen,
		  (((fg&0x08)<<3) | (bg<<3) | (fg&0x07)) << 8);
	part->cx += charslen;
	nobreak = 0;

	if (par_format.align != AL_NONE) {
		while (part->cx > overlap(par_format)
		       && part->cx > par_format.leftmargin) {
			int x;

#if 0
			if (part->cx > part->x) {
				part->x = part->cx + par_format.rightmargin;
				if (chars[charslen - 1] == ' ') part->x--;
			}
#endif
			x = split_line(part);
			if (!x) break;

#if 0
			if (LEN(part->cy-1) > part->x)
				part->x = LEN(part->cy-1);
#endif

			align_line(part, part->cy - 1, 0);
			nobreak = x - 1;
		}
	}

	part->xa += charslen;
	tmp = part->xa
	      - (chars[charslen - 1] == ' ' && par_format.align != AL_NONE)
	      + par_format.leftmargin + par_format.rightmargin;

	if (tmp > part->xmax) part->xmax = tmp;

	return;

process_link:
	if ((last_link /*|| last_target*/ || last_image || last_form)
	    && !xstrcmp(format.link, last_link)
	    && !xstrcmp(format.target, last_target)
	    && !xstrcmp(format.image, last_image)
	    && format.form == last_form) {
		if (!part->data) goto x;
		link = &part->data->links[part->data->nlinks - 1];
		if (!part->data->nlinks) { /* if this is occur,
					      then link = &part->data->links[-1];
					      it seems strange to me,
					      shouldn't we move that test before previous
					      line ? --Zas */
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

		if (d_opt->num_links_display) {
			unsigned char s[64];
			unsigned char *fl = format.link;
			unsigned char *ft = format.target;
			unsigned char *fi = format.image;
			struct form_control *ff = format.form;
			int slen = 0;

			format.link = format.target = format.image = NULL;
			format.form = NULL;

			s[slen++] = '[';
			ulongcat(s, &slen, part->link_num, sizeof(s) - 3, 0);
			s[slen++] = ']';
			s[slen] = '\0';

			put_chars(part, s, slen);

			if (ff && ff->type == FC_TEXTAREA) line_break(part);
			if (part->cx == -1) part->cx = par_format.leftmargin;
			format.link = fl;
		   	format.target = ft;
			format.image = fi;
			format.form = ff;
		}

		part->link_num++;
		last_link = format.link ? stracpy(format.link) : NULL;
		last_target = format.target ? stracpy(format.target) : NULL;
		last_image = format.image ? stracpy(format.image) : NULL;
		last_form = format.form;

		if (!part->data) goto no_l;

		link = new_link(part->data);
		if (!link) goto no_l;

		link->num = format.tabindex + part->link_num - 1;
		link->accesskey = format.accesskey;
		link->pos = NULL;

		link->title = format.title ? stracpy(format.title) : NULL;

		if (!last_form) {
			link->type = L_LINK;
			link->where = last_link ? stracpy(last_link) : NULL;
			link->target = last_target ? stracpy(last_target) : NULL;
			link->name = memacpy(chars, charslen);

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
			link->target = last_form->target ?
				       stracpy(last_form->target) : NULL;
		}

		link->where_img = last_image ? stracpy(last_image) : NULL;

		if (link->type != L_FIELD && link->type != L_AREA) {
			bg = find_nearest_color(&format.clink, 8);
			fg = find_nearest_color(&format.bg, 8);
		} else {
			fg = find_nearest_color(&format.fg, 8);
			bg = find_nearest_color(&format.bg, 8);
		}
		fg = fg_color(fg, bg);

		link->sel_color = ((fg & 8) << 3) | (fg & 7) | (bg << 3);
		link->n = 0;
set_link:
		pt = mem_realloc(link->pos,
				 (link->n + charslen) * sizeof(struct point));
		if (pt) {
			link->pos = pt;
			for (i = 0; i < charslen; i++) {
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

	/* FIXME:
	 * This doesn't work correctly with <a href="foo">123<sup>456</sup>789</a> */
	if (d_opt->display_subs) {
		if (format.attr & AT_SUBSCRIPT) {
			if (!sub) {
				sub = 1;
				put_chars(part, "[", 1);
			}
		} else {
			if (sub) {
				put_chars(part, "]", 1);
				sub = 0;
			}
		}
	}

	if (d_opt->display_sups) {
		if (format.attr & AT_SUPERSCRIPT) {
			if (!super) {
				super = 1;
				put_chars(part, "^", 1);
			}
		} else {
			if (super) {
				super = 0;
			}
		}
	}

	goto end_format_change;
}

#undef overlap

void
line_break(struct part *part)
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
	if (part->cx > par_format.leftmargin && LEN(part->cy) > part->cx - 1
	    && (POS(part->cx - 1, part->cy) & 0xff) == ' ') {
		del_chars(part, part->cx - 1, part->cy);
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
	memset(part->spaces, 0, part->spaces_len);
}

static void
html_init(struct part *part)
{
	/* !!! FIXME: background */
}

void
destroy_fc(struct form_control *fc)
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

static void
html_form_control(struct part *part, struct form_control *fc)
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

	/* We don't want to recode hidden fields. */
	if (fc->type == FC_TEXT || fc->type == FC_PASSWORD ||
	    fc->type == FC_TEXTAREA) {
#if 0
		int i;
#endif
		unsigned char *dv = convert_string(convert_table,
						   fc->default_value,
						   strlen(fc->default_value));

		if (dv) {
			if (fc->default_value) mem_free(fc->default_value);
			fc->default_value = dv;
		}

#if 0
		for (i = 0; i < fc->nvalues; i++) {
			dv = convert_string(convert_table, fc->values[i], strlen(fc->values[i]));
			if (dv) {
				mem_free(fc->values[i]);
				fc->values[i] = dv;
			}
		}
#endif
	}

	add_to_list(part->data->forms, fc);
}

static void
add_frameset_entry(struct frameset_desc *fsd,
		   struct frameset_desc *subframe,
		   unsigned char *name, unsigned char *url)
{
	int idx;

	if (fsd->yp >= fsd->y) return;

	idx = fsd->xp + fsd->yp * fsd->x;
	fsd->f[idx].subframe = subframe;
	fsd->f[idx].name = name ? stracpy(name) : NULL;
	fsd->f[idx].url = url ? stracpy(url) : NULL;
	fsd->xp++;
	if (fsd->xp >= fsd->x) {
		fsd->xp = 0;
		fsd->yp++;
	}
}

static struct frameset_desc *
create_frameset(struct f_data *fda, struct frameset_param *fp)
{
	int i;
	struct frameset_desc *fd;

	if (!fp->x || !fp->y) {
		internal("zero size of frameset");
		return NULL;
	}

	fd = mem_calloc(1, sizeof(struct frameset_desc)
			   + fp->x * fp->y * sizeof(struct frame_desc));
	if (!fd) return NULL;

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

static inline void
create_frame(struct frame_param *fp)
{
	add_frameset_entry(fp->parent, NULL, fp->name, fp->url);
}

static void *
html_special(struct part *part, enum html_special_type c, ...)
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
			va_end(l);
			break;
		case SP_CONTROL:
			fc = va_arg(l, struct form_control *);
			html_form_control(part, fc);
			va_end(l);
			break;
		case SP_TABLE:
			va_end(l);
			return convert_table;
		case SP_USED:
			va_end(l);
			return (void *)!!part->data;
		case SP_FRAMESET:
			fsp = va_arg(l, struct frameset_param *);
			va_end(l);
			return create_frameset(part->data, fsp);
		case SP_FRAME:
			fp = va_arg(l, struct frame_param *);
			va_end(l);
			create_frame(fp);
			break;
		case SP_NOWRAP:
			nowrap = va_arg(l, int);
			va_end(l);
			break;
		default:
			va_end(l);
			internal("html_special: unknown code %d", c);
	}

	return NULL;
}

static inline void
do_format(char *start, char *end, struct part *part, unsigned char *head)
{
	parse_html(start, end,
		   (void (*)(void *, unsigned char *, int)) put_chars_conv,
		   (void (*)(void *)) line_break,
		   (void (*)(void *)) html_init,
		   (void *(*)(void *, int, ...)) html_special,
		   part, head);
	/* if ((part->y -= line_breax) < 0) part->y = 0; */
}

void
free_table_cache(void)
{
	if (table_cache) {
		struct hash_item *item;
		int i;

		/* We do not free key here. */
		foreach_hash_item (item, *table_cache, i)
			if (item->value)
				mem_free(item->value);

		free_hash(table_cache);
	}

	table_cache = NULL;
	table_cache_entries = 0;
}

struct part *
format_html_part(unsigned char *start, unsigned char *end,
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

	/* Hash creation if needed. */
	if (!table_cache) {
		table_cache = init_hash(8, &strhash);
	} else if (!data) {
		/* Search for cached entry. */
		struct table_cache_entry_key key;
		struct hash_item *item;

		/* Clear key to prevent potential alignment problem
		 * when keys are compared. */
		memset(&key, 0, sizeof(struct table_cache_entry_key));

		key.start = start;
		key.end = end;
		key.align = align;
		key.m = m;
		key.width = width;
		key.xs = xs;
		key.link_num = link_num;

		item = get_hash_item(table_cache, (unsigned char *)&key,
				     sizeof(struct table_cache_entry_key));
		if (item) { /* We found it in cache, so just copy and return. */
			part = mem_alloc(sizeof(struct part));
			if (part)  {
				memcpy(part,
				       &((struct table_cache_entry *)item->value)->part,
			       	       sizeof(struct part));

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

		last_link_to_move = data->nlinks;
		last_tag_to_move = (void *)&data->tags;
		last_tag_for_newline = (void *)&data->tags;
	} else {
		last_link_to_move = 0;
		last_tag_to_move = NULL;
		last_tag_for_newline = NULL;
	}

	margin = m;
	empty_format = !data;

	if (last_link) mem_free(last_link);
	if (last_image) mem_free(last_image);
	if (last_target) mem_free(last_target);

	last_link = last_image = last_target = NULL;
	last_form = NULL;
	nobreak = 1;

	part = mem_calloc(1, sizeof(struct part));
	if (!part) goto ret;

	part->data = data;
	part->xp = xs;
	part->yp = ys;
	part->cx = -1;
	part->cy = 0;
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

	if (part->spaces) mem_free(part->spaces);

	if (data) {
		struct node *n = data->nodes.next;

		n->yw = ys - n->y + part->y;
	}

	foreach (fc, part->uf) destroy_fc(fc);
	free_list(part->uf);

ret:
	last_link_to_move = llm;
	last_tag_to_move = ltm;
	/*last_tag_for_newline = ltn;*/
	margin = lm;
	empty_format = ef;

	if (table_level > 1 && !data && table_cache
	    && table_cache_entries < MAX_TABLE_CACHE_ENTRIES) {
		/* Create a new entry. */
		/* Clear memory to prevent bad key comparaison due to alignment
		 * of key fields. */
		tce = mem_calloc(1, sizeof(struct table_cache_entry));
		/* A goto is used here to prevent a test or code
		 * redundancy. */
		if (!tce) goto end;

		tce->key.start = start;
		tce->key.end = end;
		tce->key.align = align;
		tce->key.m = m;
		tce->key.width = width;
		tce->key.xs = xs;
		tce->key.link_num = link_num;
		memcpy(&tce->part, part, sizeof(struct part));

		if (!add_hash_item(table_cache, (unsigned char *)&tce->key,
				   sizeof(struct table_cache_entry_key), tce)) {
			mem_free(tce);
		} else {
			table_cache_entries++;
		}
	}

end:
	last_link = last_image = last_target = NULL;
	last_form = NULL;

	return part;
}

static void
push_base_format(unsigned char *url, struct document_options *opt)
{
	struct html_element *e;

	if (html_stack.next != &html_stack) {
		internal("something on html stack");
		init_list(html_stack);
	}

	e = mem_calloc(1, sizeof(struct html_element));
	if (!e) return;

	add_to_list(html_stack, e);

	format.attr = 0;
	format.fontsize = 3;
	format.link = format.target = format.image = NULL;
	format.select = NULL;
	format.form = NULL;
	format.title = NULL;

	memcpy(&format.fg, &opt->default_fg, sizeof(struct rgb));
	memcpy(&format.bg, &opt->default_bg, sizeof(struct rgb));
	memcpy(&format.clink, &opt->default_link, sizeof(struct rgb));
	memcpy(&format.vlink, &opt->default_vlink, sizeof(struct rgb));

	format.href_base = stracpy(url);
	format.target_base = opt->framename ? stracpy(opt->framename) : NULL;

	if (opt->plain) {
		par_format.align = AL_NONE;
		par_format.leftmargin = 0;
		par_format.rightmargin = 0;
	} else {
		par_format.align = AL_LEFT;
		par_format.leftmargin = opt->margin;
		par_format.rightmargin = opt->margin;
	}

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

struct conv_table *
get_convert_table(unsigned char *head, int to,
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

static void
format_html(struct cache_entry *ce, struct f_data *screen)
{
	unsigned char *url = ce->url;
	struct fragment *fr;
	struct part *rp;
	unsigned char *start = NULL;
	unsigned char *end = NULL;
	unsigned char *t;
	unsigned char *head = init_str();
	int hdl = 0;
	int i;

	if (!head) return;

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

	if (ce->head) add_to_str(&head, &hdl, ce->head);

	i = d_opt->plain;
	scan_http_equiv(start, end, &head, &hdl, &t);
	convert_table = get_convert_table(head, screen->opt.cp,
					  screen->opt.assume_cp,
					  &screen->cp, &screen->ass,
					  screen->opt.hard_assume);
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
			if (screen->data[i].d) mem_free(screen->data[i].d);
			screen->y--;
		} else break;
	}

	for (i = 0; i < screen->y; i++)
		if (screen->data[i].l > screen->x)
			screen->x = screen->data[i].l;

	if (form.action) mem_free(form.action), form.action = NULL;
	if (form.target) mem_free(form.target), form.target = NULL;

	screen->bg = find_nearest_color(&par_format.bgcolor, 8) << 11;

	kill_html_stack_item(html_stack.next);

	if (html_stack.next != &html_stack) {
		internal("html stack not empty after operation");
		init_list(html_stack);
	}

	sort_links(screen);

	if (screen->frame_desc) screen->frame = 1;

#if 0 /* debug purpose */
	{
		FILE *f = fopen("forms", "a");
		struct form_control *form;
		unsigned char *qq;
		fprintf(f,"FORM:\n");
		foreach (form, screen->forms) {
			fprintf(f, "g=%d f=%d c=%d t:%d\n",
				form->g_ctrl_num, form->form_num,
				form->ctrl_num, form->type);
		}
		fprintf(f,"fragment: \n");
		for (qq = start; qq < end; qq++) fprintf(f, "%c", *qq);
		fprintf(f,"----------\n\n");
		fclose(f);
	}
#endif
}

void
shrink_format_cache(int u)
{
	struct f_data *ce;

	delete_unused_format_cache_entries();

	if (format_cache_entries < 0) {
		internal("format_cache_entries underflow");
		format_cache_entries = 0;
	}

	ce = format_cache.prev;
	while ((u || format_cache_entries > get_opt_int("document.cache.format.size"))
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

void
count_format_cache(void)
{
	struct f_data *ce;

	format_cache_entries = 0;
	foreach (ce, format_cache)
		if (!ce->refcount)
			format_cache_entries++;
}

void
delete_unused_format_cache_entries(void)
{
	struct f_data *ce;

	foreach (ce, format_cache) {
		struct cache_entry *cee = NULL;

		if (!ce->refcount) {
			if (!find_in_cache(ce->url, &cee) || !cee
			    || cee->count != ce->use_tag) {
				if (!cee) internal("file %s disappeared from cache", ce->url);
				ce = ce->prev;
				destroy_formatted(ce->next);
				format_cache_entries--;
			}
		}
	}
}

void
format_cache_reactivate(struct f_data *ce)
{
	del_from_list(ce);
	add_to_list(format_cache, ce);
}

void
cached_format_html(struct view_state *vs, struct f_data_c *screen,
		   struct document_options *opt)
{
	unsigned char *n;
	struct f_data *ce;
	struct cache_entry *cee = NULL;

	if (!vs) return;

	n = screen->name;
	screen->name = NULL;
	detach_formatted(screen);

	screen->name = n;
	screen->link_bg = NULL;
	screen->link_bg_n = 0;
	vs->f = screen;

	screen->vs = vs;
	screen->xl = screen->yl = -1;
	screen->f_data = NULL;

	if (!find_in_cache(vs->url, &cee) || !cee) {
		internal("document %s to format not found", vs->url);
		return;
	}

	foreach (ce, format_cache) {
		if (strcmp(ce->url, vs->url)
		    || compare_opt(&ce->opt, opt))
			continue;

		if (cee->count != ce->use_tag) {
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

	cee->refcount++;
	shrink_memory(0);

	ce = mem_alloc(sizeof(struct f_data));
	if (!ce) {
		cee->refcount--;
		return;
	}

	init_formatted(ce);
	ce->refcount = 1;

	ce->url = stracpy(vs->url);
	if (!ce->url) {
		mem_free(ce);
		cee->refcount--;
		return;
	}

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

long
formatted_info(int type)
{
	int i = 0;
	struct f_data *ce;

	switch (type) {
		case CI_FILES:
			foreach (ce, format_cache) i++;
			return i;
		case CI_LOCKED:
			foreach (ce, format_cache) i += !!ce->refcount;
			return i;
		default:
			internal("formatted_info: bad request");
	}

	return 0;
}

static void
add_frame_to_list(struct session *ses, struct f_data_c *fd)
{
	struct f_data_c *f;

	foreach (f, ses->scrn_frames) {
		if (f->yp > fd->yp || (f->yp == fd->yp && f->xp > fd->xp)) {
			add_at_pos(f->prev, fd);
			return;
		}
	}

	add_to_list_bottom(ses->scrn_frames, fd);
}

static struct f_data_c *
find_fd(struct session *ses, unsigned char *name,
	int depth, int x, int y)
{
	struct f_data_c *fd;

	foreachback (fd, ses->scrn_frames) {
		if (!fd->used && !strcasecmp(fd->name, name)) {
			fd->used = 1;
			fd->depth = depth;
			return fd;
		}
	}

	fd = mem_calloc(1, sizeof(struct f_data_c));
	if (!fd) return NULL;

	fd->used = 1;
	fd->name = stracpy(name);
	if (!fd->name) {
		mem_free(fd);
		return NULL;
	}
	fd->depth = depth;
	fd->xp = x;
	fd->yp = y;
	fd->search_word = &ses->search_word;

	/*add_to_list(ses->scrn_frames, fd);*/
	add_frame_to_list(ses, fd);

	return fd;
}

static struct f_data_c *
format_frame(struct session *ses, unsigned char *name,
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
	if (!find_in_cache(vs->url, &ce) || !ce) return NULL;

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
	if (fd) cached_format_html(vs, fd, o);

	return fd;
}

static void
format_frames(struct session *ses, struct frameset_desc *fsd,
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

void
html_interpret(struct session *ses)
{
	struct document_options o;
	struct f_data_c *fd;
	struct f_data_c *cf = NULL;
	struct view_state *l = NULL;

	if (!ses->screen) {
		ses->screen = mem_calloc(1, sizeof(struct f_data_c));
		if (!ses->screen) return;
		ses->screen->search_word = &ses->search_word;
	}

	if (have_location(ses)) l = &cur_loc(ses)->vs;

	init_bars_status(ses, NULL, &o);

	o.col = get_opt_bool_tree(ses->tab->term->spec, "colors");
	o.cp = get_opt_int_tree(ses->tab->term->spec, "charset");

	mk_document_options(&o);

	if (l) {
		if (l->plain < 0) l->plain = 0;
		o.plain = l->plain;
	} else {
		o.plain = 1;
	}

	memcpy(&o.default_fg, get_opt_ptr("document.colors.text"), sizeof(struct rgb));
	memcpy(&o.default_bg, get_opt_ptr("document.colors.background"), sizeof(struct rgb));
	memcpy(&o.default_link, get_opt_ptr("document.colors.link"), sizeof(struct rgb));
	memcpy(&o.default_vlink, get_opt_str("document.colors.vlink"), sizeof(struct rgb));

	o.framename = "";

	foreach (fd, ses->scrn_frames) fd->used = 0;

	cached_format_html(l, ses->screen, &o);

	if (ses->screen->f_data && ses->screen->f_data->frame) {
		cf = current_frame(ses);
		format_frames(ses, ses->screen->f_data->frame_desc, &o, 0);
	}

	foreach (fd, ses->scrn_frames) if (!fd->used) {
		struct f_data_c *fdp = fd->prev;

		detach_formatted(fd);
		del_from_list(fd);
		mem_free(fd);
		fd = fdp;
	}

	if (cf) {
		int n = 0;

		foreach (fd, ses->scrn_frames) {
			if (fd->f_data && fd->f_data->frame) continue;
			if (fd == cf) {
				cur_loc(ses)->vs.current_link = n;
				break;
			}
			n++;
		}
	}
}


static inline void
add_srch_chr(struct f_data *f, unsigned char c, int x, int y, int nn)
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
/* Debugging code, please keep it. */
void
sdbg(struct f_data *f)
{
	struct node *n;

	foreachback (n, f->nodes) {
		int xm = n->x + n->xw, ym = n->y + n->yw;
		printf("%d %d - %d %d\n", n->x, n->y, xm, ym);
		fflush(stdout);
	}
	debug("!");
}
#endif


static void
sort_srch(struct f_data *f)
{
	int i;
	int *min, *max;

	f->slines1 = mem_calloc(f->y, sizeof(struct search *));
	if (!f->slines1) return;

	f->slines2 = mem_calloc(f->y, sizeof(struct search *));
	if (!f->slines2) {
		mem_free(f->slines1);
		return;
	}

	min = mem_calloc(f->y, sizeof(int));
	if (!min) {
		mem_free(f->slines1);
		mem_free(f->slines2);
		return;
	}

	max = mem_calloc(f->y, sizeof(int));
	if (!max) {
		mem_free(f->slines1);
		mem_free(f->slines2);
		mem_free(min);
		return;
	}

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

static int
get_srch(struct f_data *f)
{
	struct node *n;
	int cnt = 0;
	int cc = !f->search;

	foreachback (n, f->nodes) {
		int x, y;
		int xm = n->x + n->xw;
		int ym = n->y + n->yw;

#if 0
		printf("%d %d - %d %d\n", n->x, n->y, xm, ym);
		fflush(stdout);
#endif
#define ADD(cr, nn) if (!cc) add_srch_chr(f, (cr), x, y, (nn)); else cnt++;

		for (y = n->y; y < ym && y < f->y; y++) {
			int ns = 1;

			for (x = n->x; x < xm && x < f->data[y].l; x++) {
				unsigned char c = f->data[y].d[x];

				if (c < ' ') c = ' ';
				if (c == ' ' && ns) continue;

				if (ns) {
					ADD(c, 1);
					ns = 0;
					continue;
				}

				if (c != ' ') {
					ADD(c, 1);
				} else {
					int xx;
					int found = 0;

					for (xx = x + 1;
					     xx < xm && xx < f->data[y].l;
					     xx++) {
						if ((unsigned char) f->data[y].d[xx] >= ' ') {
							found = 1;
							break;
						}
					}

					if (found) {
						ADD(' ', xx - x);
						x = xx - 1;
					} else {
						ADD(' ', 0);
						break;
					}
				}

			}

			ADD(' ', 0);
		}
#undef ADD

	}

	return cnt;
}

void
get_search_data(struct f_data *f)
{
	int n;

	if (f->search) return;

	n = get_srch(f);
	if (!n) return;

	f->nsearch = 0;

	f->search = mem_alloc(n * sizeof(struct search));
	if (!f->search) return;

	get_srch(f);
	while (f->nsearch && f->search[f->nsearch - 1].c == ' ') f->nsearch--;
	sort_srch(f);
}
