/* HTML renderer */
/* $Id: renderer.c,v 1.240 2003/09/07 00:10:15 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <stdarg.h>
#include <string.h>

#include "elinks.h"

#include "main.h"
#include "config/options.h"
#include "document/cache.h"
#include "document/options.h"
#include "document/html/frames.h"
#include "document/html/parser.h"
#include "document/html/renderer.h"
#include "document/html/tables.h"
#include "intl/charsets.h"
#include "lowlevel/ttime.h"
#include "protocol/http/header.h"
#include "protocol/uri.h"
#include "sched/session.h"
#include "terminal/color.h"
#include "terminal/draw.h"
#include "util/color.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/hash.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/form.h"
#include "viewer/text/link.h"
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

#define LINES_GRANULARITY	0x7F
#define LINE_GRANULARITY	0x0F

#define ALIGN(x, gr)	(((x) + (gr)) & ~(gr))
#define ALIGN_LINES(x)	ALIGN(x, LINES_GRANULARITY)
#define ALIGN_LINE(x)	ALIGN(x, LINE_GRANULARITY)

static int nowrap = 0; /* Activated/deactivated by SP_NOWRAP. */
static int sub = 0; /* Activated/deactivated by AT_SUBSCRIPT */
static int super = 0; /* Activated/deactivated by AT_SUPERSCRIPT */


static int
realloc_lines(struct document *document, int y)
{
	int newsize = ALIGN_LINES(y + 1);
	int oldsize = ALIGN_LINES(document->y);
	struct line *lines;

	assert(document);
	if_assert_failed return 0;

	lines = document->data;

	if (newsize > oldsize) {
		lines = mem_realloc(lines, newsize * sizeof(struct line));
		if (!lines) return -1;

		document->data = lines;
		memset(&lines[oldsize], 0,
		       (newsize - oldsize) * sizeof(struct line));
	}

	document->y = y + 1;

	return 0;
}

static int
realloc_line(struct document *document, int y, int x)
{
	int i;
	int newsize = ALIGN_LINE(x + 1);
	struct line *line;
	struct color_pair colors = INIT_COLOR_PAIR(par_format.bgcolor, 0x0);
	struct screen_char schar = INIT_SCREEN_CHAR(' ', 0, 0);

	assert(document);
	if_assert_failed return 0;

	line = &document->data[y];

	if (newsize > ALIGN_LINE(line->l)) {
		struct screen_char *l;

		l = mem_realloc(line->d, newsize * sizeof(struct screen_char));
		if (!l)	return -1;

		line->d = l;
	}

	set_term_color8(&schar, &colors, 8, 16);

	for (i = line->l; i <= x; i++) {
		memcpy(&line->d[i], &schar, sizeof(struct screen_char));
	}

	line->l = i;

	return 0;
}

static inline int
xpand_lines(struct part *p, int y)
{
	assert(p && p->document);
	if_assert_failed return 0;

	y += p->yp;
	if (y >= p->document->y) return realloc_lines(p->document, y);

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
	assert(p && p->document && p->document->data);
	if_assert_failed return 0;

	x += p->xp;
	y += p->yp;

	assertm(y < p->document->y, "line does not exist");
	if_assert_failed return 0;

	if (x < p->document->data[y].l)
		return 0;

	return realloc_line(p->document, y, x);
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


#define X(x)		(part->xp + (x))
#define Y(y)		(part->yp + (y))
#define LINE(y)		part->document->data[Y(y)]
#define POS(x, y)	LINE(y).d[X(x)]
#define LEN(y)		int_max(LINE(y).l - part->xp, 0)
#define SLEN(y, x)	do { LINE(y).l = X(x); } while (0)


/* If @bgcolor is NULL don't touch any color. */
static inline void
set_hchars(struct part *part, int x, int y, int xl,
	   unsigned char data, color_t *bgcolor, enum screen_char_attr attr)
{
	assert(part && part->document);
	if_assert_failed return;

	if (xpand_lines(part, y)
	    || xpand_line(part, y, x + xl - 1))
		return;

	assert(part->document->data);
	if_assert_failed return;

	if (bgcolor) {
		struct color_pair colors = INIT_COLOR_PAIR(*bgcolor, 0x0);
		struct screen_char schar = INIT_SCREEN_CHAR(data, attr, 0);

		set_term_color8(&schar, &colors, 8, 16);
		schar.data = data;

		for (; xl; xl--, x++) {
			memcpy(&POS(x, y), &schar, sizeof(struct screen_char));
		}
	} else {
		for (; xl; xl--, x++) {
			POS(x, y).data = data;
			POS(x, y).attr = attr;
		}
	}
}

void
xset_hchar(struct part *part, int x, int y,
	   unsigned char data, color_t bgcolor, enum screen_char_attr attr)
{
	struct color_pair colors = INIT_COLOR_PAIR(bgcolor, 0x0);

	assert(part && part->document);
	if_assert_failed return;

	if (xpand_lines(part, y)
	    || xpand_line(part, y, x))
		return;

	assert(part->document->data);
	if_assert_failed return;

	POS(x, y).data = data;
	set_term_color8(&POS(x, y), &colors, 8, 16);
}

void
xset_hchars(struct part *part, int x, int y, int xl,
	    unsigned char data, color_t bgcolor, enum screen_char_attr attr)
{
	set_hchars(part, x, y, xl, data, &bgcolor, attr);
}

void
xset_vchars(struct part *part, int x, int y, int yl,
	    unsigned char data, color_t bgcolor, enum screen_char_attr attr)
{
	struct color_pair colors = INIT_COLOR_PAIR(bgcolor, 0x0);
	struct screen_char schar = INIT_SCREEN_CHAR(data, attr, 0);

	assert(part && part->document);
	if_assert_failed return;

	if (xpand_lines(part, y + yl - 1))
		return;

	assert(part->document->data);
	if_assert_failed return;

	set_term_color8(&schar, &colors, 8, 16);

	for (; yl; yl--, y++) {
	    	if (xpand_line(part, y, x)) return;

		memcpy(&POS(x, y), &schar, sizeof(struct screen_char));
	}
}

static inline void
set_hline(struct part *part, int x, int y, unsigned char *chars,
	  int charslen, unsigned char color, enum screen_char_attr attr)
{
	assert(part);
	if_assert_failed return;

	if (xpand_spaces(part, x + charslen - 1))
		return;

	if (part->document) {
		if (xpand_lines(part, y)
		    || xpand_line(part, y, x + charslen - 1))
			return;

		for (; charslen > 0; charslen--, x++, chars++) {
			part->spaces[x] = (*chars == ' ');
			POS(x, y).color = color;
			POS(x, y).attr = attr;
			POS(x, y).data = *chars;
		}
	} else {
		for (; charslen > 0; charslen--, x++, chars++) {
			part->spaces[x] = (*chars == ' ');
		}
	}
}

static void
move_links(struct part *part, int xf, int yf, int xt, int yt)
{
	struct tag *tag;
	int nlink;
	int matched = 0;

	assert(part && part->document);
	if_assert_failed return;
	xpand_lines(part, yt);

	for (nlink = last_link_to_move; nlink < part->document->nlinks; nlink++) {
		struct link *link = &part->document->links[nlink];
		int i;

		for (i = 0; i < link->n; i++) {
			if (link->pos[i].y == Y(yf)) {
				matched = 1;
				if (link->pos[i].x >= X(xf)) {
					if (yt >= 0) {
						link->pos[i].y = Y(yt);
						link->pos[i].x += -xf + xt;
					} else if (i < link->n - 1) {
						memmove(&link->pos[i],
							&link->pos[i + 1],
							(link->n - i - 1) *
							sizeof(struct point));
						link->n--;
						i--;
					} else {
						 /* assert(i < (link->n-1));
						  * ABORT? */
					}
				}
			}
		}

		if (!matched) last_link_to_move = nlink;
	}

	matched = 0;

	if (yt >= 0) {
		for (tag = last_tag_to_move->next;
	   	     (void *) tag != &part->document->tags;
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
copy_chars(struct part *part, int x, int y, int xl, struct screen_char *d)
{
	assert(xl > 0 && part && part->document && part->document->data);
	if_assert_failed return;

	if (xpand_lines(part, y)
	    || xpand_line(part, y, x + xl - 1))
		return;

	memcpy(&POS(x, y), d, xl * sizeof(struct screen_char));
}

static inline void
move_chars(struct part *part, int x, int y, int nx, int ny)
{
	assert(part && part->document && part->document->data);
	if_assert_failed return;

	if (LEN(y) - x <= 0) return;
	copy_chars(part, nx, ny, LEN(y) - x, &POS(x, y));
	SLEN(y, x);
	move_links(part, x, y, nx, ny);
}

static inline void
shift_chars(struct part *part, int y, int shift)
{
	struct screen_char *a;
	int len;

	assert(part && part->document && part->document->data);
	if_assert_failed return;

	len = LEN(y);

	a = mem_alloc(len * sizeof(struct screen_char));
	if (!a) return;

	memcpy(a, &POS(0, y), len * sizeof(struct screen_char));
	/* When we shift chars we want to preserve and use the background
	 * colors already in place else we could end up ``staining'' the background
	 * especial when drawing table cells. So make the shifted chars share the
	 * colors in place. */
	set_hchars(part, 0, y, shift, ' ', NULL, 0);
	copy_chars(part, shift, y, len, a);
	mem_free(a);

	move_links(part, 0, y, shift, y);
}

static inline void
del_chars(struct part *part, int x, int y)
{
	assert(part && part->document && part->document->data);
	if_assert_failed return;

	SLEN(y, x);
	move_links(part, x, y, -1, -1);
}

#define overlap(x) ((x).width - (x).rightmargin > 0 ? (x).width - (x).rightmargin : 0)

static int inline
split_line_at(struct part *part, register int x)
{
	register int tmp;
	int new_x = x + par_format.rightmargin;

	assert(part);
	if_assert_failed return 0;

	/* Make sure that we count the right margin to the total
	 * actual box width. */
	if (new_x > part->x)
		part->x = new_x;

	if (part->document) {
		assert(part->document->data);
		if_assert_failed return 0;
		assertm(POS(x, part->cy).data == ' ',
			"bad split: %c", POS(x, part->cy).data);
		move_chars(part, x + 1, part->cy, par_format.leftmargin, part->cy + 1);
		del_chars(part, x, part->cy);
	}

	x++; /* Since we were using (x + 1) only later... */

	tmp = part->spaces_len - x;
	if (tmp > 0) {
		/* 0 is possible and I'm paranoid ... --Zas */
		memmove(part->spaces, part->spaces + x, tmp);
	}

	assert(tmp >= 0);
	if_assert_failed tmp = 0;
	memset(part->spaces + tmp, 0, x);

	if (par_format.leftmargin > 0) {
		tmp = part->spaces_len - par_format.leftmargin;
		assertm(tmp > 0, "part->spaces_len - par_format.leftmargin == %d", tmp);
		/* So tmp is zero, memmove() should survive that. Don't recover. */
		memmove(part->spaces + par_format.leftmargin, part->spaces, tmp);
	}

	part->cy++;

	if (part->cx == x) {
		part->cx = -1;
		int_lower_bound(&part->y, part->cy);
		return 2;
	} else {
		part->cx -= x - par_format.leftmargin;
		int_lower_bound(&part->y, part->cy + 1);
		return 1;
	}
}

/* Here, we scan the line for a possible place where we could split it into two
 * (breaking it, because it is too long), if it is overlapping from the maximal
 * box width. */
/* Returns 0 if there was found no spot suitable for breaking the line.
 *         1 if the line was split into two.
 *         2 if the (second) splitted line is blank (that is useful to determine
 *           ie. if the next line_break() should really break the line; we don't
 *           want to see any blank lines to pop up, do we?). */
static int
split_line(struct part *part)
{
	register int x;

	assert(part);
	if_assert_failed return 0;

	for (x = overlap(par_format); x >= par_format.leftmargin; x--)
		if (x < part->spaces_len && part->spaces[x])
			return split_line_at(part, x);

	for (x = par_format.leftmargin; x < part->cx ; x++)
		if (x < part->spaces_len && part->spaces[x])
			return split_line_at(part, x);

	/* Make sure that we count the right margin to the total
	 * actual box width. */
	int_lower_bound(&part->x, part->cx + par_format.rightmargin);

	return 0;
}

/* This function is very rare exemplary of clean and beautyful code here.
 * Please handle with care. --pasky */
static void
justify_line(struct part *part, int y)
{
	struct screen_char *line; /* we save original line here */
	int len;
	int pos;
	int *space_list;
	int spaces;

	assert(part && part->document && part->document->data);
	if_assert_failed return;

	len = LEN(y);
	assert(len > 0);
	if_assert_failed return;

	line = fmem_alloc(len * sizeof(struct screen_char));
	if (!line) return;

	/* It may sometimes happen that the line is only one char long and that
	 * char is space - then we're going to write to both [0] and [1], but
	 * we allocated only one field. Thus, we've to do (len + 1). --pasky */
	space_list = fmem_alloc((len + 1) * sizeof(int));
	if (!space_list) {
		fmem_free(line);
		return;
	}

	memcpy(line, &POS(0, y), len * sizeof(struct screen_char));

	/* Skip leading spaces */

	spaces = 0;
	pos = 0;

	while (line[pos].data == ' ')
		pos++;

	/* Yes, this can be negative, we know. But we add one to it always
	 * anyway, so it's ok. */
	space_list[spaces++] = pos - 1;

	/* Count spaces */

	for (; pos < len; pos++)
		if (line[pos].data == ' ')
			space_list[spaces++] = pos;

	space_list[spaces] = len;

	/* Realign line */

	if (spaces > 1) {
		int insert = overlap(par_format) - len;
		int prev_end = 0;
		int word;

		/* See shift_chars() about why we pass a NULL bg color. */
		set_hchars(part, 0, y, overlap(par_format), ' ', NULL, 0);

		for (word = 0; word < spaces; word++) {
			/* We have to increase line length by 'insert' num. of
			 * characters, so we move 'word'th word 'word_shift'
			 * characters right. */
			int word_start = space_list[word] + 1;
			int word_len = space_list[word + 1] - word_start;
			int word_shift;
			int new_start;

			assert(word_len >= 0);
			if_assert_failed continue;
			if (!word_len) continue;

			word_shift = (word * insert) / (spaces - 1);
			new_start = word_start + word_shift;

			copy_chars(part, new_start, y, word_len,
				   &line[word_start]);

			/* There are now (new_start - prev_end) spaces before
			 * the word. */
			if (word)
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

	assert(part && part->document && part->document->data);
	if_assert_failed return;

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
new_link(struct document *f)
{
	assert(f);
	if_assert_failed return NULL;

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
html_tag(struct document *f, unsigned char *t, int x, int y)
{
	struct tag *tag;
	int tsize;

	assert(f);
	if_assert_failed return;

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


static void
put_chars_conv(struct part *part, unsigned char *chars, int charslen)
{
	unsigned char *buffer;

	assert(part && chars);
	if_assert_failed return;

	if (format.attr & AT_GRAPHICS) {
		put_chars(part, chars, charslen);
		return;
	}

	if (!charslen) {
		put_chars(part, NULL, 0);
		return;
	}

	/* XXX: Perhaps doing the whole string at once could be an ugly memory
	 * hit? Dunno, someone should measure that. --pasky */

	buffer = convert_string(convert_table, chars, charslen, CSM_DEFAULT);
	if (buffer) {
		put_chars(part, buffer, strlen(buffer));
		mem_free(buffer);
	}
}

static void
put_chars_format_change(struct part *part, unsigned char *color,
			enum screen_char_attr *attr)
{
	static struct text_attrib_beginning ta_cache = { -1, 0x0, 0x0 };
	static struct screen_char schar_cache;
	struct color_pair colors;

	if (!memcmp(&ta_cache, &format, sizeof(struct text_attrib_beginning))) {
		*color = schar_cache.color;
		*attr = schar_cache.attr;
		return;
	}

	colors.background = format.bg;
	colors.foreground = format.fg;

	*attr = 0;
	if (format.attr) {
		if (format.attr & AT_UNDERLINE) {
			*attr |= SCREEN_ATTR_UNDERLINE;
		}

		if (format.attr & AT_BOLD) {
			*attr |= SCREEN_ATTR_BOLD;
		}

		if (format.attr & AT_ITALIC) {
			*attr |= SCREEN_ATTR_ITALIC;
		}

		if (format.attr & AT_GRAPHICS) {
			*attr |= SCREEN_ATTR_FRAME;
		}
	}

	memcpy(&ta_cache, &format, sizeof(struct text_attrib_beginning));
	memset(&schar_cache, 0, sizeof(struct screen_char));
	set_term_color8(&schar_cache, &colors, 8, 16);
	*color = schar_cache.color;
	*attr = schar_cache.attr;

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
}

static inline void
put_link_number(struct part *part)
{
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

static inline void
process_link(struct part *part, unsigned char *chars, int charslen)
{
	struct link *link;
	struct point *pt;

	if ((last_link || last_image || last_form)
	    && !xstrcmp(format.link, last_link)
	    && !xstrcmp(format.target, last_target)
	    && !xstrcmp(format.image, last_image)
	    && format.form == last_form) {
		if (!part->document) return;

		assertm(part->document->nlinks > 0, "no link");
		if_assert_failed return;

		link = &part->document->links[part->document->nlinks - 1];

	} else {
		if (last_link) mem_free(last_link);	/* !!! FIXME: optimize */
		if (last_target) mem_free(last_target);
		if (last_image) mem_free(last_image);

		last_link = last_target = last_image = NULL;
		last_form = NULL;

		if (!(format.link || format.image || format.form)) return;

		if (d_opt->num_links_display) {
			put_link_number(part);
		}

		part->link_num++;
		last_link = format.link ? stracpy(format.link) : NULL;
		last_target = format.target ? stracpy(format.target) : NULL;
		last_image = format.image ? stracpy(format.image) : NULL;
		last_form = format.form;

		if (!part->document) return;

		link = new_link(part->document);
		if (!link) return;

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
			link->color.background = format.clink;
			link->color.foreground = format.bg;
		} else {
			link->color.foreground = format.fg;
			link->color.background = format.bg;
		}

		link->n = 0;
	}

	pt = mem_realloc(link->pos, (link->n + charslen) * sizeof(struct point));
	if (pt) {
		register int i;

		link->pos = pt;
		for (i = 0; i < charslen; i++) {
			pt[link->n + i].x = X(part->cx) + i;
			pt[link->n + i].y = Y(part->cy);
		}
		link->n += i;
	}
}

void
put_chars(struct part *part, unsigned char *chars, int charslen)
{
	unsigned char color;
	enum screen_char_attr attr = 0;

	assert(part);
	if_assert_failed return;

	assert(chars);
	if_assert_failed return;

	while (par_format.align != AL_NONE && part->cx == -1
	       && charslen && *chars == ' ') {
		chars++;
		charslen--;
	}

	if (!charslen) return;

	if (chars[0] != ' ' || (chars[1] && chars[1] != ' ')) {
		last_tag_for_newline = (void *)&part->document->tags;
	}
	if (part->cx == -1) part->cx = par_format.leftmargin;

	if (last_link || last_image || last_form || format.link
	    || format.image || format.form)
		process_link(part, chars, charslen);

	put_chars_format_change(part, &color, &attr);

	if (part->cx == par_format.leftmargin && *chars == ' '
	    && par_format.align != AL_NONE) {
		chars++;
		charslen--;
	}

	if (!charslen) return;

	int_lower_bound(&part->y, part->cy + 1);

	if (nowrap && part->cx + charslen > overlap(par_format))
		return;

	set_hline(part, part->cx, part->cy, chars, charslen, color, attr);
	part->cx += charslen;
	nobreak = 0;

	if (par_format.align != AL_NONE) {
		while (part->cx > overlap(par_format)
		       && part->cx > par_format.leftmargin) {
			int x = split_line(part);

			if (!x) break;
			if (part->document)
				align_line(part, part->cy - 1, 0);
			nobreak = x - 1;
		}
	}

	assert(charslen > 0);
	part->xa += charslen;
	part->xmax = int_max(part->xmax, part->xa
					 - (chars[charslen - 1] == ' '
				            && par_format.align != AL_NONE)
					 + par_format.leftmargin
					 + par_format.rightmargin);
	return;

}

#undef overlap

void
line_break(struct part *part)
{
	struct tag *t;

	assert(part);
	if_assert_failed return;

	int_lower_bound(&part->x, part->cx + par_format.rightmargin);

	if (nobreak) {
		nobreak = 0;
		part->cx = -1;
		part->xa = 0;
		return;
	}

	if (!part->document || !part->document->data) goto end;

	xpand_lines(part, part->cy + 1);
	if (part->cx > par_format.leftmargin && LEN(part->cy) > part->cx - 1
	    && POS(part->cx - 1, part->cy).data == ' ') {
		del_chars(part, part->cx - 1, part->cy);
		part->cx--;
	}

	if (part->cx > 0) align_line(part, part->cy, 1);

	for (t = last_tag_for_newline;
	     t && (void *)t != &part->document->tags;
	     t = t->prev) {
		t->x = X(0);
		t->y = Y(part->cy + 1);
	}

end:
	part->cy++;
	part->cx = -1;
	part->xa = 0;
   	memset(part->spaces, 0, part->spaces_len);
}

static void
html_init(struct part *part)
{
	assert(part);
	/* !!! FIXME: background */
}

static void
html_form_control(struct part *part, struct form_control *fc)
{
	assert(part && fc);
	if_assert_failed return;

	if (!part->document) {
		done_form_control(fc);
		mem_free(fc);
		return;
	}

	fc->g_ctrl_num = g_ctrl_num++;

	/* We don't want to recode hidden fields. */
	if (fc->type == FC_TEXT || fc->type == FC_PASSWORD ||
	    fc->type == FC_TEXTAREA) {
		unsigned char *dv = convert_string(convert_table,
						   fc->default_value,
						   strlen(fc->default_value), CSM_QUERY);

		if (dv) {
			if (fc->default_value) mem_free(fc->default_value);
			fc->default_value = dv;
		}
	}

	add_to_list(part->document->forms, fc);
}


static void *
html_special(struct part *part, enum html_special_type c, ...)
{
	va_list l;
	unsigned char *t;
	struct document *document = part->document;
	unsigned long seconds;
	struct form_control *fc;
	struct frameset_param *fsp;
	struct frame_param *fp;

	assert(part);
	if_assert_failed return NULL;

	va_start(l, c);
	switch (c) {
		case SP_TAG:
			t = va_arg(l, unsigned char *);
			if (document)
				html_tag(document, t, X(part->cx), Y(part->cy));
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
			return (void *)!!document;
		case SP_FRAMESET:
			fsp = va_arg(l, struct frameset_param *);
			va_end(l);
			return create_frameset(document, fsp);
		case SP_FRAME:
			fp = va_arg(l, struct frame_param *);
			va_end(l);
			create_frame(fp);
			break;
		case SP_NOWRAP:
			nowrap = va_arg(l, int);
			va_end(l);
			break;
		case SP_REFRESH:
			seconds = va_arg(l, unsigned long);
			t = va_arg(l, unsigned char *);
			va_end(l);
			document->refresh = init_document_refresh(t, seconds);
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
		 int align, int m, int width, struct document *document,
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
	struct table_cache_entry *tce;

	/* Hash creation if needed. */
	if (!table_cache) {
		table_cache = init_hash(8, &strhash);
	} else if (!document) {
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

	assertm(ys >= 0, "format_html_part: ys == %d", ys);
	if_assert_failed return NULL;

	if (document) {
		struct node *n = mem_alloc(sizeof(struct node));

		if (n) {
			n->x = xs;
			n->y = ys;
			n->xw = !table_level ? MAXINT : width;
			add_to_list(document->nodes, n);
		}

		last_link_to_move = document->nlinks;
		last_tag_to_move = (void *)&document->tags;
		last_tag_for_newline = (void *)&document->tags;
	} else {
		last_link_to_move = 0;
		last_tag_to_move = NULL;
		last_tag_for_newline = NULL;
	}

	margin = m;
	empty_format = !document;

	if (last_link) mem_free(last_link);
	if (last_image) mem_free(last_image);
	if (last_target) mem_free(last_target);

	last_link = last_image = last_target = NULL;
	last_form = NULL;
	nobreak = 1;

	part = mem_calloc(1, sizeof(struct part));
	if (!part) goto ret;

	part->document = document;
	part->xp = xs;
	part->yp = ys;
	part->cx = -1;
	part->cy = 0;
	part->link_num = link_num;

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

	part->xmax = int_max(part->xmax, part->x);

	nobreak = 0;
	line_breax = 1;

	if (last_link) mem_free(last_link);
	if (last_image) mem_free(last_image);
	if (last_target) mem_free(last_target);

	while (&html_top != e) {
		kill_html_stack_item(&html_top);
#if 0
		/* I've preserved this bit to show an example of the Old Code
		 * of the Mikulas days (I _HOPE_ it's by Mikulas, at least ;-).
		 * I think this assert() can never fail, for one. --pasky */
		assertm(&html_top && (void *)&html_top != (void *)&html_stack,
			"html stack trashed");
		if_assert_failed break;
#endif
	}

	html_top.dontkill = 0;
	kill_html_stack_item(&html_top);

	if (part->spaces) mem_free(part->spaces);

	if (document) {
		struct node *n = document->nodes.next;

		n->yw = ys - n->y + part->y;
	}

ret:
	last_link_to_move = llm;
	last_tag_to_move = ltm;
	/*last_tag_for_newline = ltn;*/
	margin = lm;
	empty_format = ef;

	if (table_level > 1 && !document && table_cache
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

	assert(url && opt);
	if_assert_failed return;
	assertm(list_empty(html_stack), "something on html stack");
	if_assert_failed init_list(html_stack);

	e = mem_calloc(1, sizeof(struct html_element));
	if (!e) return;

	add_to_list(html_stack, e);

	format.attr = 0;
	format.fontsize = 3;
	format.link = format.target = format.image = NULL;
	format.select = NULL;
	format.form = NULL;
	format.title = NULL;

	format.fg = opt->default_fg;
	format.bg = opt->default_bg;
	format.clink = opt->default_link;
	format.vlink = opt->default_vlink;

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

	par_format.bgcolor = opt->default_bg;

	html_top.invisible = 0;
	html_top.name = NULL;
   	html_top.namelen = 0;
	html_top.options = NULL;
	html_top.linebreak = 1;
	html_top.dontkill = 1;
}

struct conv_table *
get_convert_table(unsigned char *head, int to_cp,
		  int default_cp, int *from_cp,
		  enum cp_status *cp_status, int ignore_server_cp)
{
	unsigned char *a, *b;
	unsigned char *part = head;
	int from = -1;

	assert(head);
	if_assert_failed return NULL;

	while (from == -1) {
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

	if (from == -1) {
		a = parse_http_header(head, "Content-Charset", NULL);
		if (a) {
			from = get_cp_index(a);
			mem_free(a);
		}
	}

	if (from == -1) {
		a = parse_http_header(head, "Charset", NULL);
		if (a) {
			from = get_cp_index(a);
			mem_free(a);
		}
	}

	if (cp_status) {
		if (from == -1)
			*cp_status = CP_STATUS_ASSUMED;
		else {
			if (ignore_server_cp)
				*cp_status = CP_STATUS_IGNORED;
			else
				*cp_status = CP_STATUS_SERVER;
		}
	}

	if (ignore_server_cp || from == -1) from = default_cp;
	if (from_cp) *from_cp = from;

	return get_translation_table(from, to_cp);
}

static void
format_html(struct cache_entry *ce, struct document *screen)
{
	struct fragment *fr;
	struct part *rp;
	unsigned char *url;
	unsigned char *start = NULL;
	unsigned char *end = NULL;
	struct string title;
	struct string head;
	int i;

	assert(ce && screen);
	if_assert_failed return;

	if (!init_string(&head)) return;

	url = ce->url;
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

	if (ce->head) add_to_string(&head, ce->head);

	i = d_opt->plain;
	scan_http_equiv(start, end, &head, &title);
	convert_table = get_convert_table(head.source, screen->opt.cp,
					  screen->opt.assume_cp,
					  &screen->cp,
					  &screen->cp_status,
					  screen->opt.hard_assume);
	d_opt->plain = 0;
	screen->title = convert_string(convert_table, title.source, title.length, CSM_DEFAULT);
	d_opt->plain = i;
	done_string(&title);

	push_base_format(url, &screen->opt);

	table_level = 0;
	g_ctrl_num = 0;
	last_form_tag = NULL;
	last_form_attr = NULL;
	last_input_tag = NULL;

	rp = format_html_part(start, end, par_format.align,
			      par_format.leftmargin, screen->opt.xw, screen,
			      0, 0, head.source, 1);
	if (rp) mem_free(rp);

	done_string(&head);

	screen->x = 0;

	for (i = screen->y - 1; i >= 0; i--) {
		if (!screen->data[i].l) {
			if (screen->data[i].d) mem_free(screen->data[i].d);
			screen->y--;
		} else break;
	}

	for (i = 0; i < screen->y; i++)
		screen->x = int_max(screen->x, screen->data[i].l);

	if (form.action) mem_free(form.action), form.action = NULL;
	if (form.target) mem_free(form.target), form.target = NULL;

	screen->bgcolor = par_format.bgcolor;

	kill_html_stack_item(html_stack.next);

	assertm(list_empty(html_stack),
		"html stack not empty after operation");
	if_assert_failed init_list(html_stack);

	sort_links(screen);

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
	struct document *ce;

	delete_unused_format_cache_entries();

	assertm(format_cache_entries >= 0, "format_cache_entries underflow");
	if_assert_failed format_cache_entries = 0;

	ce = format_cache.prev;
	while ((u || format_cache_entries > get_opt_int("document.cache.format.size"))
	       && (void *)ce != &format_cache) {

		if (ce->refcount) {
			ce = ce->prev;
			continue;
		}

		ce = ce->prev;
		done_document(ce->next);
		format_cache_entries--;
	}
}

void
count_format_cache(void)
{
	struct document *ce;

	format_cache_entries = 0;
	foreach (ce, format_cache)
		if (!ce->refcount)
			format_cache_entries++;
}

void
delete_unused_format_cache_entries(void)
{
	struct document *ce;

	foreach (ce, format_cache) {
		struct cache_entry *cee = NULL;

		if (!ce->refcount) {
			if (!find_in_cache(ce->url, &cee) || !cee
			    || cee->count != ce->use_tag) {
				assertm(cee, "file %s disappeared from cache",
					ce->url);
				ce = ce->prev;
				done_document(ce->next);
				format_cache_entries--;
			}
		}
	}
}

void
format_cache_reactivate(struct document *ce)
{
	assert(ce);
	if_assert_failed return;

	del_from_list(ce);
	add_to_list(format_cache, ce);
}

void
cached_format_html(struct view_state *vs, struct document_view *document_view,
		   struct document_options *options)
{
	unsigned char *name;
	struct document *document;
	struct cache_entry *cache_entry = NULL;

	assert(vs && document_view && options);
	if_assert_failed return;

	name = document_view->name;
	document_view->name = NULL;
	detach_formatted(document_view);

	document_view->name = name;
	document_view->link_bg = NULL;
	document_view->link_bg_n = 0;
	vs->view = document_view;

	document_view->vs = vs;
	document_view->xl = document_view->yl = -1;
	document_view->document = NULL;

	if (!find_in_cache(vs->url, &cache_entry) || !cache_entry) {
		internal("document %s to format not found", vs->url);
		return;
	}

	foreach (document, format_cache) {
		if (strcmp(document->url, vs->url)
		    || compare_opt(&document->opt, options))
			continue;

		if (cache_entry->count != document->use_tag) {
			if (!document->refcount) {
				document = document->prev;
				done_document(document->next);
				format_cache_entries--;
			}
			continue;
		}

		format_cache_reactivate(document);

		if (!document->refcount++) format_cache_entries--;
		document_view->document = document;

		goto sx;
	}

	cache_entry->refcount++;
	shrink_memory(0);

	document = init_document(vs->url, options);
	if (!document) {
		cache_entry->refcount--;
		return;
	}

	add_to_list(format_cache, document);

	document_view->document = document;
	format_html(cache_entry, document);

sx:
	document_view->xw = document->opt.xw;
	document_view->yw = document->opt.yw;
	document_view->xp = document->opt.xp;
	document_view->yp = document->opt.yp;
}

long
formatted_info(int type)
{
	int i = 0;
	struct document *ce;

	switch (type) {
		case INFO_FILES:
			foreach (ce, format_cache) i++;
			return i;
		case INFO_LOCKED:
			foreach (ce, format_cache) i += !!ce->refcount;
			return i;
		default:
			internal("formatted_info: bad request");
	}

	return 0;
}


void
html_interpret(struct session *ses)
{
	struct document_options o;
	struct document_view *fd;
	struct document_view *cf = NULL;
	struct view_state *l = NULL;

	if (!ses->screen) {
		ses->screen = mem_calloc(1, sizeof(struct document_view));
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

	o.default_fg = get_opt_color("document.colors.text");
	o.default_bg = get_opt_color("document.colors.background");
	o.default_link = get_opt_color("document.colors.link");
	o.default_vlink = get_opt_color("document.colors.vlink");

	o.framename = "";

	foreach (fd, ses->scrn_frames) fd->used = 0;

	if (l) cached_format_html(l, ses->screen, &o);

	if (ses->screen->document && ses->screen->document->frame_desc) {
		cf = current_frame(ses);
		format_frames(ses, ses->screen->document->frame_desc, &o, 0);
	}

	foreach (fd, ses->scrn_frames) if (!fd->used) {
		struct document_view *fdp = fd->prev;

		detach_formatted(fd);
		del_from_list(fd);
		mem_free(fd);
		fd = fdp;
	}

	if (cf) {
		int n = 0;

		foreach (fd, ses->scrn_frames) {
			if (fd->document && fd->document->frame_desc) continue;
			if (fd == cf) {
				cur_loc(ses)->vs.current_link = n;
				break;
			}
			n++;
		}
	}
}
