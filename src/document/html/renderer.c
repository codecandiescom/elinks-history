/* HTML renderer */
/* $Id: renderer.c,v 1.345 2003/10/30 16:49:21 jonas Exp $ */

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
#include "document/document.h"
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

enum link_state {
	LINK_STATE_NONE,
	LINK_STATE_NEW,
	LINK_STATE_SAME,
};

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

static int table_cache_entries = 0;
static int last_link_to_move;
static struct tag *last_tag_to_move;
static struct tag *last_tag_for_newline;
static unsigned char *last_link;
static unsigned char *last_target;
static unsigned char *last_image;
static struct form_control *last_form;
static int nobreak;
static int nowrap = 0; /* Activated/deactivated by SP_NOWRAP. */
static struct conv_table *convert_table;
static int g_ctrl_num;
static int empty_format;

static struct hash *table_cache = NULL;


/* Prototypes */
void line_break(struct part *);
void put_chars(struct part *, unsigned char *, int);

#define X(x_)	(part->x + (x_))
#define Y(y_)	(part->y + (y_))

#define LINES_GRANULARITY	0x7F
#define LINE_GRANULARITY	0x0F
#define LINK_GRANULARITY	0x7F
#define SPACES_GRANULARITY	0x7F

#define ALIGN_LINES(x, o, n) mem_align_alloc(x, o, n, sizeof(struct line), LINES_GRANULARITY)
#define ALIGN_LINE(x, o, n) mem_align_alloc(x, o, n, sizeof(struct screen_char), LINE_GRANULARITY)
#define ALIGN_LINK(x, o, n) mem_align_alloc(x, o, n, sizeof(struct link), LINK_GRANULARITY)
#define ALIGN_SPACES(x, o, n) mem_align_alloc(x, o, n, sizeof(unsigned char), SPACES_GRANULARITY)

static int
realloc_lines(struct document *document, int y)
{
	assert(document);
	if_assert_failed return 0;

	if (document->height <= y) {
		if (!ALIGN_LINES(&document->data, document->height, y + 1))
			return -1;

		document->height = y + 1;
	}

	return 0;
}

static int
realloc_line(struct document *document, int y, int x)
{
	struct color_pair colors = INIT_COLOR_PAIR(par_format.bgcolor, 0x0);
	struct screen_char *pos, *end;
	struct line *line;

	if (realloc_lines(document, y))
		return -1;

	line = &document->data[y];

	if (x < line->l)
		return 0;

	if (!ALIGN_LINE(&line->d, line->l, x + 1))
		return -1;

	/* Make a template of the last char using that align alloc clears the
	 * other members. */
	end = &line->d[x];
	end->data = ' ';
	set_term_color(end, &colors, 0, document->options.color_mode);

	for (pos = &line->d[line->l]; pos < end; pos++) {
		copy_screen_chars(pos, end, 1);
	}

	line->l = x + 1;

	return 0;
}

int
expand_lines(struct part *part, int y)
{
	assert(part && part->document);
	if_assert_failed return -1;

	return realloc_lines(part->document, Y(y));
}

int
expand_line(struct part *part, int y, int x)
{
	assert(part && part->document);
	if_assert_failed return -1;

	return realloc_line(part->document, Y(y), X(x));
}

static inline int
realloc_spaces(struct part *part, int length)
{
	if (length < part->spaces_len)
		return 0;

	if (!ALIGN_SPACES(&part->spaces, part->spaces_len, length))
		return -1;

	part->spaces_len = length;

	return 0;
}


#define LINE(y_)	part->document->data[Y(y_)]
#define POS(x_, y_)	LINE(y_).d[X(x_)]
#define LEN(y_)		int_max(LINE(y_).l - part->x, 0)


/* When we clear chars we want to preserve and use the background colors
 * already in place else we could end up ``staining'' the background especial
 * when drawing table cells. So make the cleared chars share the colors in
 * place. */
static inline void
clear_hchars(struct part *part, int x, int y, int xl)
{
	assert(part && part->document && xl > 0);
	if_assert_failed return;

	if (realloc_line(part->document, Y(y), X(x) + xl - 1))
		return;

	assert(part->document->data);
	if_assert_failed return;

	for (; xl; xl--, x++) {
		POS(x, y).data = ' ';
		POS(x, y).attr = 0;
	}
}

/* TODO: Merge parts with get_format_screen_char(). --jonas */
/* Allocates the required chars on the given line and returns the char at
 * position (x, y) ready to be used as a template char.  */
static inline struct screen_char *
get_frame_char(struct part *part, int x, int y, unsigned char data)
{
	struct color_pair colors = INIT_COLOR_PAIR(par_format.bgcolor, 0x0);
	struct screen_char *template;
	static enum color_flags color_flags;
	static enum color_mode color_mode;

	assert(part && part->document && x >= 0 && y >= 0);
	if_assert_failed return NULL;

	if (realloc_line(part->document, Y(y), X(x)))
		return NULL;

	assert(part->document->data);
	if_assert_failed return NULL;

	template = &POS(x, y);
	template->data = data;
	template->attr = SCREEN_ATTR_FRAME;

	color_mode = part->document->options.color_mode;
	color_flags = part->document->options.color_flags;

	set_term_color(template, &colors, color_flags, color_mode);

	return template;
}

void
draw_frame_hchars(struct part *part, int x, int y, int xl, unsigned char data)
{
	struct screen_char *template = get_frame_char(part, x + xl - 1, y, data);

	assert(xl > 0);
	if_assert_failed return;

	if (!template) return;

	/* The template char is the last we need to draw so only decrease @xl. */
	for (xl -= 1; xl; xl--, x++) {
		copy_screen_chars(&POS(x, y), template, 1);
	}
}

void
draw_frame_vchars(struct part *part, int x, int y, int yl, unsigned char data)
{
	struct screen_char *template = get_frame_char(part, x, y, data);

	if (!template) return;

	/* The template char is the first vertical char to be drawn. So
	 * copy it to the rest. */
	for (yl -= 1, y += 1; yl; yl--, y++) {
	    	if (realloc_line(part->document, Y(y), X(x)))
			return;

		copy_screen_chars(&POS(x, y), template, 1);
	}
}

static inline struct screen_char *
get_format_screen_char(struct part *part, enum link_state link_state)
{
	static struct text_attrib_beginning ta_cache = { -1, 0x0, 0x0 };
	static struct screen_char schar_cache;

	if (memcmp(&ta_cache, &format, sizeof(struct text_attrib_beginning))) {
		struct color_pair colors = INIT_COLOR_PAIR(format.bg, format.fg);
		static enum color_mode color_mode;
		static enum color_flags color_flags;

		if (d_opt) {
			color_mode = d_opt->color_mode;
			color_flags = d_opt->color_flags;
		}

		schar_cache.attr = 0;
		if (format.attr) {
			if (format.attr & AT_UNDERLINE) {
				schar_cache.attr |= SCREEN_ATTR_UNDERLINE;
			}

			if (format.attr & AT_BOLD) {
				schar_cache.attr |= SCREEN_ATTR_BOLD;
			}

			if (format.attr & AT_ITALIC) {
				schar_cache.attr |= SCREEN_ATTR_ITALIC;
			}

			if (format.attr & AT_GRAPHICS) {
				schar_cache.attr |= SCREEN_ATTR_FRAME;
			}
		}

		if (link_state != LINK_STATE_NONE
		    && d_opt->underline_links) {
			schar_cache.attr |= SCREEN_ATTR_UNDERLINE;
		}

		memcpy(&ta_cache, &format, sizeof(struct text_attrib_beginning));
		set_term_color(&schar_cache, &colors, color_flags, color_mode);

		if (d_opt->display_subs) {
			static int sub = 0;

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
			static int super = 0;

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

	return &schar_cache;
}

/* First possibly do the format change and then find out what coordinates
 * to use since sub- or superscript might change them */
static inline void
set_hline(struct part *part, unsigned char *chars, int charslen,
	  enum link_state link_state)
{
	struct screen_char *schar = get_format_screen_char(part, link_state);
	int x = part->cx;
	int y = part->cy;

	assert(part);
	if_assert_failed return;

	if (realloc_spaces(part, x + charslen))
		return;

	if (part->document) {
		if (realloc_line(part->document, Y(y), X(x) + charslen - 1))
			return;

		for (; charslen > 0; charslen--, x++, chars++) {
			part->spaces[x] = (*chars == ' ');
			schar->data = *chars;
			copy_screen_chars(&POS(x, y), schar, 1);
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
	int nlink = last_link_to_move;
	int matched = 0;

	assert(part && part->document);
	if_assert_failed return;

	if (realloc_lines(part->document, Y(yt)))
		return;

	for (; nlink < part->document->nlinks; nlink++) {
		struct link *link = &part->document->links[nlink];
		int i;

		for (i = 0; i < link->n; i++) {
			if (link->pos[i].y != Y(yf))
				continue;

			matched = 1;

			if (link->pos[i].x < X(xf))
				continue;

			if (yt >= 0) {
				link->pos[i].y = Y(yt);
				link->pos[i].x += -xf + xt;
				continue;
			} else if (i < link->n - 1) {
				memmove(&link->pos[i],
					&link->pos[i + 1],
					(link->n - i - 1) *
					sizeof(struct point));
				link->n--;
				i--;
			}
		}

		if (!matched) last_link_to_move = nlink;
	}

	/* Don't move tags when removing links. */
	if (yt < 0) return;

	matched = 0;
	tag = last_tag_to_move->next;

	for (; (void *) tag != &part->document->tags; tag = tag->next) {
		if (tag->y == Y(yf)) {
			matched = 1;
			if (tag->x >= X(xf)) {
				tag->y = Y(yt), tag->x += -xf + xt;
			}
		}

		if (!matched) last_tag_to_move = tag;
	}
}

static inline void
copy_chars(struct part *part, int x, int y, int xl, struct screen_char *d)
{
	assert(xl > 0 && part && part->document && part->document->data);
	if_assert_failed return;

	if (realloc_line(part->document, Y(y), X(x) + xl - 1))
		return;

	copy_screen_chars(&POS(x, y), d, xl);
}

static inline void
move_chars(struct part *part, int x, int y, int nx, int ny)
{
	assert(part && part->document && part->document->data);
	if_assert_failed return;

	if (LEN(y) - x <= 0) return;
	copy_chars(part, nx, ny, LEN(y) - x, &POS(x, y));

	LINE(y).l = X(x);
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

	copy_screen_chars(a, &POS(0, y), len);

	clear_hchars(part, 0, y, shift);
	copy_chars(part, shift, y, len, a);
	mem_free(a);

	move_links(part, 0, y, shift, y);
}

static inline void
del_chars(struct part *part, int x, int y)
{
	assert(part && part->document && part->document->data);
	if_assert_failed return;

	LINE(y).l = X(x);
	move_links(part, x, y, -1, -1);
}

#define overlap(x) ((x).width - (x).rightmargin > 0 ? (x).width - (x).rightmargin : 0)

static int inline
split_line_at(struct part *part, register int width)
{
	register int tmp;
	int new_width = width + par_format.rightmargin;

	assert(part);
	if_assert_failed return 0;

	/* Make sure that we count the right margin to the total
	 * actual box width. */
	if (new_width > part->width)
		part->width = new_width;

	if (part->document) {
		assert(part->document->data);
		if_assert_failed return 0;
		assertm(POS(width, part->cy).data == ' ',
			"bad split: %c", POS(width, part->cy).data);
		move_chars(part, width + 1, part->cy, par_format.leftmargin, part->cy + 1);
		del_chars(part, width, part->cy);
	}

	width++; /* Since we were using (x + 1) only later... */

	tmp = part->spaces_len - width;
	if (tmp > 0) {
		/* 0 is possible and I'm paranoid ... --Zas */
		memmove(part->spaces, part->spaces + width, tmp);
	}

	assert(tmp >= 0);
	if_assert_failed tmp = 0;
	memset(part->spaces + tmp, 0, width);

	if (par_format.leftmargin > 0) {
		tmp = part->spaces_len - par_format.leftmargin;
		assertm(tmp > 0, "part->spaces_len - par_format.leftmargin == %d", tmp);
		/* So tmp is zero, memmove() should survive that. Don't recover. */
		memmove(part->spaces + par_format.leftmargin, part->spaces, tmp);
	}

	part->cy++;

	if (part->cx == width) {
		part->cx = -1;
		int_lower_bound(&part->height, part->cy);
		return 2;
	} else {
		part->cx -= width - par_format.leftmargin;
		int_lower_bound(&part->height, part->cy + 1);
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
	int_lower_bound(&part->width, part->cx + par_format.rightmargin);

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

	copy_screen_chars(line, &POS(0, y), len);

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

		clear_hchars(part, 0, y, overlap(par_format));

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
new_link(struct document *document, int link_number,
	 unsigned char *name, int namelen)
{
	struct link *link;

	assert(document);
	if_assert_failed return NULL;

	if (!ALIGN_LINK(&document->links, document->nlinks, document->nlinks + 1))
		return NULL;

	link = &document->links[document->nlinks++];
	link->num = link_number - 1;
	if (document->options.use_tabindex) link->num += format.tabindex;
	link->accesskey = format.accesskey;
	link->title = format.title ? stracpy(format.title) : NULL;
	link->where_img = format.image ? stracpy(format.image) : NULL;

	if (!format.form) {
		link->type = L_LINK;
		link->where = format.link ? stracpy(format.link) : NULL;
		link->target = format.target ? stracpy(format.target) : NULL;
		link->name = memacpy(name, namelen);

	} else {
		struct form_control *form = format.form;

		switch (form->type) {
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
		link->form = form;
		link->target = form->target ? stracpy(form->target) : NULL;
	}

	link->color.background = format.bg;
	link->color.foreground = !link_is_textinput(link)
				? format.clink : format.fg;

	return link;
}

static void
html_tag(struct document *document, unsigned char *t, int x, int y)
{
	struct tag *tag;
	int tag_len;

	assert(document);
	if_assert_failed return;

	tag_len = strlen(t);
	/* One byte is reserved for name in struct tag. */
	tag = mem_alloc(sizeof(struct tag) + tag_len);
	if (tag) {
		tag->x = x;
		tag->y = y;
		memcpy(tag->name, t, tag_len + 1);
		add_to_list(document->tags, tag);
		if ((void *) last_tag_for_newline == &document->tags)
			last_tag_for_newline = tag;
	}
}


static void
put_chars_conv(struct part *part, unsigned char *chars, int charslen)
{
	unsigned char *buffer;

	assert(part && chars && charslen);
	if_assert_failed return;

	if (format.attr & AT_GRAPHICS) {
		put_chars(part, chars, charslen);
		return;
	}

	/* XXX: Perhaps doing the whole string at once could be an ugly memory
	 * hit? Dunno, someone should measure that. --pasky */

	buffer = convert_string(convert_table, chars, charslen, CSM_DEFAULT);
	if (buffer) {
		if (*buffer) put_chars(part, buffer, strlen(buffer));
		mem_free(buffer);
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

	/* We might have ended up on a new line after the line breaking
	 * or putting the link number chars. */
	if (part->cx == -1) part->cx = par_format.leftmargin;

	format.link = fl;
	format.target = ft;
	format.image = fi;
	format.form = ff;
}

#define realloc_points(link, size) \
	mem_align_alloc(&(link)->pos, (link)->n, size, sizeof(struct point), 0)

static inline void
process_link(struct part *part, enum link_state link_state,
	     unsigned char *chars, int charslen)
{
	struct link *link;

	if (link_state == LINK_STATE_SAME) {
		if (!part->document) return;

		assertm(part->document->nlinks > 0, "no link");
		if_assert_failed return;

		link = &part->document->links[part->document->nlinks - 1];

	} else {
		assert(link_state == LINK_STATE_NEW);

		part->link_num++;

		last_link = format.link ? stracpy(format.link) : NULL;
		last_target = format.target ? stracpy(format.target) : NULL;
		last_image = format.image ? stracpy(format.image) : NULL;
		last_form = format.form;

		if (!part->document) return;

		link = new_link(part->document, part->link_num, chars, charslen);
		if (!link) return;
	}

	/* Add new canvas positions to the link. */
	if (realloc_points(link, link->n + charslen)) {
		struct point *point = &link->pos[link->n];
		int x = X(part->cx);
		int y = Y(part->cy);

		link->n += charslen;

		for (; charslen > 0; charslen--, point++, x++) {
			point->x = x;
			point->y = y;
		}
	}
}

static inline enum link_state
get_link_state(void)
{
	enum link_state state;

	if (!(format.link || format.image || format.form)) {
		state = LINK_STATE_NONE;

	} else if ((last_link || last_image || last_form)
		   && !xstrcmp(format.link, last_link)
		   && !xstrcmp(format.target, last_target)
		   && !xstrcmp(format.image, last_image)
		   && format.form == last_form) {

		return LINK_STATE_SAME;

	} else {
		state = LINK_STATE_NEW;
	}

	if (last_link) mem_free(last_link);
	if (last_target) mem_free(last_target);
	if (last_image) mem_free(last_image);

	last_link = last_target = last_image = NULL;
	last_form = NULL;

	return state;
}

void
put_chars(struct part *part, unsigned char *chars, int charslen)
{
	enum link_state link_state;

	assert(part);
	if_assert_failed return;

	assert(chars && charslen);
	if_assert_failed return;

	/* If we are not handling verbatim aligning and we are at the begining
	 * of a line trim whitespace. */
	if (part->cx == -1) {
		/* If we are not handling verbatim aligning trim whitespace. */
		if  (par_format.align != AL_NONE) {
			while (charslen && *chars == ' ') {
				chars++;
				charslen--;
			}

			if (charslen < 1) return;
		}

		part->cx = par_format.leftmargin;
	}

	if (chars[0] != ' ' || (charslen > 1 && chars[1] != ' ')) {
		last_tag_for_newline = (void *)&part->document->tags;
	}

	int_lower_bound(&part->height, part->cy + 1);

	link_state = get_link_state();

	if (d_opt->num_links_display && link_state == LINK_STATE_NEW) {
		put_link_number(part);
	}

	set_hline(part, chars, charslen, link_state);

	if (link_state != LINK_STATE_NONE) {
		process_link(part, link_state, chars, charslen);
	}

	if (nowrap && part->cx + charslen > overlap(par_format))
		return;

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
	int_lower_bound(&part->max_width, part->xa
			+ par_format.leftmargin + par_format.rightmargin
			- (chars[charslen - 1] == ' '
			   && par_format.align != AL_NONE));
	return;

}

#undef overlap

void
line_break(struct part *part)
{
	struct tag *t;

	assert(part);
	if_assert_failed return;

	int_lower_bound(&part->width, part->cx + par_format.rightmargin);

	if (nobreak) {
		nobreak = 0;
		part->cx = -1;
		part->xa = 0;
		return;
	}

	if (!part->document || !part->document->data) goto end;

	if (realloc_lines(part->document, part->height + part->cy + 1))
		return;

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

static inline void
color_link_lines(struct document *document)
{
	struct color_pair colors = INIT_COLOR_PAIR(par_format.bgcolor, 0x0);
	enum color_mode color_mode = document->options.color_mode;
	enum color_flags color_flags = document->options.color_flags;
	int y;

	for (y = 0; y < document->height; y++) {
		int x;

		for (x = 0; x < document->data[y].l; x++) {
			struct screen_char *schar = &document->data[y].d[x];

			set_term_color(schar, &colors, color_flags, color_mode);

			/* XXX: Entering hack zone! Change to clink color after
			 * link text has been recolored. */
			if (schar->data == ':' && colors.foreground == 0x0)
				colors.foreground = format.clink;
		}

		colors.foreground = 0x0;
	}
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
		case SP_COLOR_LINK_LINES:
			va_end(l);
			if (document && document->options.use_document_colours == 2)
				color_link_lines(document);
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
	struct html_element *html_state;
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
		struct node *node = mem_alloc(sizeof(struct node));

		if (node) {
			node->x = xs;
			node->y = ys;
			node->width = !table_level ? MAXINT : width;
			add_to_list(document->nodes, node);
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
	part->x = xs;
	part->y = ys;
	part->cx = -1;
	part->cy = 0;
	part->link_num = link_num;

	html_state = init_html_parser_state(align, m, width);

	do_format(start, end, part, head);

	done_html_parser_state(html_state);

	part->max_width = int_max(part->max_width, part->width);

	nobreak = 0;

	if (last_link) mem_free(last_link);
	if (last_image) mem_free(last_image);
	if (last_target) mem_free(last_target);
	if (part->spaces) mem_free(part->spaces);

	if (document) {
		struct node *node = document->nodes.next;

		node->height = ys - node->y + part->height;
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
format_html(struct cache_entry *ce, struct document *document)
{
	struct fragment *fr;
	struct part *rp;
	unsigned char *url;
	unsigned char *start = NULL;
	unsigned char *end = NULL;
	struct string title;
	struct string head;
	int i;

	assert(ce && document);
	if_assert_failed return;

	if (!init_string(&head)) return;

	g_ctrl_num = 0;
	url = ce->url;
	d_opt = &document->options;
	document->id_tag = ce->id_tag;
	defrag_entry(ce);
	fr = ce->frag.next;

	if (!((void *)fr == &ce->frag || fr->offset || !fr->length)) {
		start = fr->data;
		end = fr->data + fr->length;
	}

	if (ce->head) add_to_string(&head, ce->head);

	init_html_parser(url, &document->options, start, end, &head, &title,
			 (void (*)(void *, unsigned char *, int)) put_chars_conv,
			 (void (*)(void *)) line_break,
			 (void (*)(void *)) html_init,
			 (void *(*)(void *, enum html_special_type, ...)) html_special);

	i = d_opt->plain;
	convert_table = get_convert_table(head.source, document->options.cp,
					  document->options.assume_cp,
					  &document->cp,
					  &document->cp_status,
					  document->options.hard_assume);
	d_opt->plain = 0;
	document->title = convert_string(convert_table, title.source, title.length, CSM_DEFAULT);
	d_opt->plain = i;
	done_string(&title);

	rp = format_html_part(start, end, par_format.align,
			      par_format.leftmargin, document->options.width, document,
			      0, 0, head.source, 1);
	if (rp) mem_free(rp);

	done_string(&head);

	document->width = 0;

	for (i = document->height - 1; i >= 0; i--) {
		if (!document->data[i].l) {
			if (document->data[i].d) mem_free(document->data[i].d);
			document->height--;
		} else break;
	}

	for (i = 0; i < document->height; i++)
		document->width = int_max(document->width, document->data[i].l);

	document->bgcolor = par_format.bgcolor;

	done_html_parser();

	sort_links(document);

#if 0 /* debug purpose */
	{
		FILE *f = fopen("forms", "a");
		struct form_control *form;
		unsigned char *qq;
		fprintf(f,"FORM:\n");
		foreach (form, document->forms) {
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

	document_view->vs = vs;
	document_view->last_x = document_view->last_y = -1;
	document_view->document = NULL;

	if (!find_in_cache(vs->url, &cache_entry) || !cache_entry) {
		internal("document %s to format not found", vs->url);
		return;
	}

	foreach (document, format_cache) {
		if (strcmp(document->url, vs->url)
		    || compare_opt(&document->options, options))
			continue;

		if (cache_entry->id_tag != document->id_tag) {
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
	document_view->width = document->options.width;
	document_view->height = document->options.height;
	document_view->x = document->options.x;
	document_view->y = document->options.y;
}


void
html_interpret(struct session *ses)
{
	struct document_options o;
	struct document_view *doc_view;
	struct document_view *current_doc_view = NULL;
	struct view_state *l = NULL;

	if (!ses->doc_view) {
		ses->doc_view = mem_calloc(1, sizeof(struct document_view));
		if (!ses->doc_view) return;
		ses->doc_view->search_word = &ses->search_word;
	}

	if (have_location(ses)) l = &cur_loc(ses)->vs;

	init_document_options(&o);

	/* XXX: Sets 0.yw and 0.xw so keep after init_document_options(). */
	init_bars_status(ses, NULL, &o);

	o.color_mode = get_opt_int_tree(ses->tab->term->spec, "colors");
	if (!get_opt_int_tree(ses->tab->term->spec, "underline"))
		o.color_flags |= COLOR_ENHANCE_UNDERLINE;

	o.cp = get_opt_int_tree(ses->tab->term->spec, "charset");

	if (l) {
		if (l->plain < 0) l->plain = 0;
		o.plain = l->plain;
	} else {
		o.plain = 1;
	}

	foreach (doc_view, ses->scrn_frames) doc_view->used = 0;

	if (l) cached_format_html(l, ses->doc_view, &o);

	if (document_has_frames(ses->doc_view->document)) {
		current_doc_view = current_frame(ses);
		format_frames(ses, ses->doc_view->document->frame_desc, &o, 0);
	}

	foreach (doc_view, ses->scrn_frames) {
		struct document_view *prev_doc_view = doc_view->prev;

		if (doc_view->used) continue;

		detach_formatted(doc_view);
		del_from_list(doc_view);
		mem_free(doc_view);
		doc_view = prev_doc_view;
	}

	if (current_doc_view) {
		int n = 0;

		foreach (doc_view, ses->scrn_frames) {
			if (document_has_frames(doc_view->document)) continue;
			if (doc_view == current_doc_view) {
				cur_loc(ses)->vs.current_link = n;
				break;
			}
			n++;
		}
	}
}
