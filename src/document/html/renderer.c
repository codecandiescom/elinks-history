/* HTML renderer */
/* $Id: renderer.c,v 1.432 2004/05/10 01:51:04 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <ctype.h>
#include <stdarg.h>
#include <string.h>

#include "elinks.h"

#include "config/options.h"
#include "cache/cache.h"
#include "document/docdata.h"
#include "document/document.h"
#include "document/options.h"
#include "document/refresh.h"
#include "document/html/frames.h"
#include "document/html/parser/parse.h"
#include "document/html/parser.h"
#include "document/html/renderer.h"
#include "document/html/tables.h"
#include "intl/charsets.h"
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
#include "util/ttime.h"
#include "viewer/text/form.h"
#include "viewer/text/view.h"
#include "viewer/text/vs.h"

/* Unsafe macros */
#include "document/html/internal.h"

/* Types and structs */

enum link_state {
	LINK_STATE_NONE,
	LINK_STATE_NEW,
	LINK_STATE_SAME,
};

struct link_state_info {
	unsigned char *link;
	unsigned char *target;
	unsigned char *image;
	struct form_control *form;
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
static struct link_state_info link_state_info;
static int nobreak;
static int nosearchable;
static int nowrap = 0; /* Activated/deactivated by SP_NOWRAP. */
static struct conv_table *convert_table;
static int g_ctrl_num;
static int empty_format;
static int did_subscript = 0;

static struct hash *table_cache = NULL;


/* Prototypes */
void line_break(struct part *);
void put_chars(struct part *, unsigned char *, int);

#define X(x_)	(part->x + (x_))
#define Y(y_)	(part->y + (y_))

#define SPACES_GRANULARITY	0x7F

#define ALIGN_SPACES(x, o, n) mem_align_alloc(x, o, n, unsigned char, SPACES_GRANULARITY)

static int
realloc_line(struct document *document, int y, int x)
{
	struct color_pair colors = INIT_COLOR_PAIR(par_format.bgcolor, 0x0);
	struct screen_char *pos, *end;
	struct line *line;

	if (!realloc_lines(document, y))
		return -1;

	line = &document->data[y];

	if (x < line->length)
		return 0;

	if (!ALIGN_LINE(&line->chars, line->length, x + 1))
		return -1;

	/* We cannot rely on the aligned allocation to clear the members for us
	 * since for line splitting we simply trim the length. Question is if
	 * it is better to to clear the line after the splitting or here. */
	end = &line->chars[x];
	end->data = ' ';
	end->attr = 0;
	set_term_color(end, &colors, 0, document->options.color_mode);

	for (pos = &line->chars[line->length]; pos < end; pos++) {
		copy_screen_chars(pos, end, 1);
	}

	line->length = x + 1;

	return 0;
}

int
expand_lines(struct part *part, int y)
{
	assert(part && part->document);
	if_assert_failed return -1;

	return -!realloc_lines(part->document, Y(y));
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
#define POS(x_, y_)	LINE(y_).chars[X(x_)]
#define LEN(y_)		int_max(LINE(y_).length - part->x, 0)


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
get_frame_char(struct part *part, int x, int y, unsigned char data, color_t bgcolor, color_t fgcolor)
{
	struct color_pair colors = INIT_COLOR_PAIR(bgcolor, fgcolor);
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
draw_frame_hchars(struct part *part, int x, int y, int xl, unsigned char data, color_t bgcolor, color_t fgcolor)
{
	struct screen_char *template = get_frame_char(part, x + xl - 1, y, data, bgcolor, fgcolor);

	assert(xl > 0);
	if_assert_failed return;

	if (!template) return;

	/* The template char is the last we need to draw so only decrease @xl. */
	for (xl -= 1; xl; xl--, x++) {
		copy_screen_chars(&POS(x, y), template, 1);
	}
}

void
draw_frame_vchars(struct part *part, int x, int y, int yl, unsigned char data, color_t bgcolor, color_t fgcolor)
{
	struct screen_char *template = get_frame_char(part, x, y, data, bgcolor, fgcolor);

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

		if (global_doc_opts) {
			color_mode = global_doc_opts->color_mode;
			color_flags = global_doc_opts->color_flags;
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
		    && global_doc_opts->underline_links) {
			schar_cache.attr |= SCREEN_ATTR_UNDERLINE;
		}

		memcpy(&ta_cache, &format, sizeof(struct text_attrib_beginning));
		set_term_color(&schar_cache, &colors, color_flags, color_mode);

		if (global_doc_opts->display_subs) {
			if (format.attr & AT_SUBSCRIPT) {
				if (!did_subscript) {
					did_subscript = 1;
					put_chars(part, "[", 1);
				}
			} else {
				if (did_subscript) {
					put_chars(part, "]", 1);
					did_subscript = 0;
				}
			}
		}

		if (global_doc_opts->display_sups) {
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

	if (!!(schar_cache.attr & SCREEN_ATTR_UNSEARCHABLE) ^ !!nosearchable) {
		schar_cache.attr ^= SCREEN_ATTR_UNSEARCHABLE;
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
			if (*chars == NBSP_CHAR) {
				schar->data = ' ';
				part->spaces[x] = global_doc_opts->wrap_nbsp;
			} else {
				part->spaces[x] = (*chars == ' ');
				schar->data = *chars;
			}

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

	if (!realloc_lines(part->document, Y(yt)))
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
			} else {
				int to_move = link->n - (i + 1);

				assert(to_move >= 0);

				if (to_move > 0) {
					memmove(&link->pos[i],
						&link->pos[i + 1],
						to_move *
						sizeof(struct point));
					i--;
				}

				link->n--;
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

	LINE(y).length = X(x);
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

	LINE(y).length = X(x);
	move_links(part, x, y, -1, -1);
}

#define overlap(x) int_max((x).width - (x).rightmargin, 0)

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
			if (word) {
				/* The out commented code is a proposed fix for
				 * bug 60. The align test page seems to work
				 * but it failes for
				 * http://www.chez.com/aikidossiers/francais/origines.htm
				 */
#if 0
				int new_spaces = new_start - prev_end - 1;
				struct link *link = part->document->nlinks > 0
					? &part->document->links[part->document->nlinks - 1]
					: NULL;
#endif
				move_links(part, prev_end + 1, y, new_start, y);
#if 0
				/* FIXME: Move to move_links() --jonas */
				if (new_spaces
				    && link
				    && link->pos[link->n - 1].x < new_start
				    && link->pos[link->n - 1].y >= y
				    && realloc_points(link, link->n + new_spaces)) {
					struct point *point = &link->pos[link->n];
					int x = prev_end + 1;

					link->n += new_spaces;

					for (; new_spaces > 0; new_spaces--, point++, x++) {
						point->x = x;
						point->y = y;
					}
				}
#endif
			}

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
	link->title = null_or_stracpy(format.title);
	link->where_img = null_or_stracpy(format.image);

	if (!format.form) {
		link->type = LINK_HYPERTEXT;
		link->where = null_or_stracpy(format.link);
		link->target = null_or_stracpy(format.target);
		link->name = memacpy(name, namelen);

	} else {
		struct form_control *form = format.form;

		switch (form->type) {
		case FC_TEXT:
		case FC_PASSWORD:
		case FC_FILE:
			link->type = LINK_FIELD;
			break;
		case FC_TEXTAREA:
			link->type = LINK_AREA;
			break;
		case FC_CHECKBOX:
		case FC_RADIO:
			link->type = LINK_CHECKBOX;
			break;
		case FC_SELECT:
			link->type = LINK_SELECT;
			break;
		case FC_SUBMIT:
		case FC_IMAGE:
		case FC_RESET:
		case FC_HIDDEN:
			link->type = LINK_BUTTON;
		}
		link->form = form;
		link->target = null_or_stracpy(form->target);
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

	nosearchable = 1;
	put_chars(part, s, slen);
	nosearchable = 0;

	if (ff && ff->type == FC_TEXTAREA) line_break(part);

	/* We might have ended up on a new line after the line breaking
	 * or putting the link number chars. */
	if (part->cx == -1) part->cx = par_format.leftmargin;

	format.link = fl;
	format.target = ft;
	format.image = fi;
	format.form = ff;
}

#define assert_link_variable(old, new) \
	assertm(!(old), "Old link value [%s]. New value [%s]", old, new);

static inline void
init_link_state_info(unsigned char *link, unsigned char *target,
		     unsigned char *image, struct form_control *form)
{
	assert_link_variable(link_state_info.image, image);
	assert_link_variable(link_state_info.target, target);
	assert_link_variable(link_state_info.link, link);

	link_state_info.link = null_or_stracpy(link);
	link_state_info.target = null_or_stracpy(target);
	link_state_info.image = null_or_stracpy(image);
	link_state_info.form = format.form;
}

static inline void
done_link_state_info(void)
{
	mem_free_if(link_state_info.link);
	mem_free_if(link_state_info.target);
	mem_free_if(link_state_info.image);
	memset(&link_state_info, 0, sizeof(struct link_state_info));
}

static inline void
process_link(struct part *part, enum link_state link_state,
	     unsigned char *chars, int charslen)
{
	struct link *link;
	int x_offset = 0;

	if (link_state == LINK_STATE_SAME) {
		if (!part->document) return;

		assertm(part->document->nlinks > 0, "no link");
		if_assert_failed return;

		link = &part->document->links[part->document->nlinks - 1];

		if (link->name) {
			unsigned char *new_name;

			new_name = straconcat(link->name, chars, NULL);
			if (new_name) {
				mem_free(link->name);
				link->name = new_name;
			}
		}

	} else {
		assert(link_state == LINK_STATE_NEW);

		part->link_num++;

		init_link_state_info(format.link, format.target,
				     format.image, format.form);
		if (!part->document) return;

		/* Trim leading space from the link text */
		while (x_offset < charslen && chars[x_offset] <= ' ')
			x_offset++;

		if (x_offset) {
			charslen -= x_offset;
			chars += x_offset;
		}

		link = new_link(part->document, part->link_num, chars, charslen);
		if (!link) return;
	}

	/* Add new canvas positions to the link. */
	if (realloc_points(link, link->n + charslen)) {
		struct point *point = &link->pos[link->n];
		int x = X(part->cx) + x_offset;
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

	} else if ((link_state_info.link || link_state_info.image || link_state_info.form)
		   && !xstrcmp(format.link, link_state_info.link)
		   && !xstrcmp(format.target, link_state_info.target)
		   && !xstrcmp(format.image, link_state_info.image)
		   && format.form == link_state_info.form) {

		return LINK_STATE_SAME;

	} else {
		state = LINK_STATE_NEW;
	}

	done_link_state_info();

	return state;
}

#define is_drawing_subs_or_sups() \
	((format.attr & AT_SUBSCRIPT && global_doc_opts->display_subs) \
	 || (format.attr & AT_SUPERSCRIPT && global_doc_opts->display_sups))

void
put_chars(struct part *part, unsigned char *chars, int charslen)
{
	enum link_state link_state;
	int update_after_subscript = did_subscript;

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

	if (global_doc_opts->num_links_display && link_state == LINK_STATE_NEW) {
		put_link_number(part);
	}

	set_hline(part, chars, charslen, link_state);

	if (link_state != LINK_STATE_NONE) {
		/* We need to update the current @link_state because <sub> and
		 * <sup> tags will output to the canvas using an inner
		 * put_chars() call which results in their process_link() call
		 * will ``update'' the @link_state. */
		if (link_state == LINK_STATE_NEW
		    && (is_drawing_subs_or_sups() || update_after_subscript != did_subscript)) {
			link_state = get_link_state();
		}

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

	if (!realloc_lines(part->document, part->height + part->cy + 1))
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

		if (dv) mem_free_set(&fc->default_value, dv);
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

		for (x = 0; x < document->data[y].length; x++) {
			struct screen_char *schar = &document->data[y].chars[x];

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
		{
			struct frameset_param *fsp = va_arg(l, struct frameset_param *);
			struct frameset_desc *frameset_desc;

			va_end(l);
			if (!fsp->parent && document->frame_desc) return NULL;

			frameset_desc = create_frameset(fsp);
			if (!fsp->parent && !document->frame_desc)
				document->frame_desc = frameset_desc;
			return frameset_desc;
		}
		case SP_FRAME:
		{
			struct frameset_desc *parent = va_arg(l, struct frameset_desc *);
			unsigned char *name = va_arg(l, unsigned char *);
			unsigned char *url = va_arg(l, unsigned char *);

			va_end(l);

			add_frameset_entry(parent, NULL, name, url);
		}
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
			if (document && use_document_bg_colors(&document->options))
				color_link_lines(document);
			break;
		case SP_STYLESHEET:
		{
			unsigned char *url = va_arg(l, unsigned char *);

			va_end(l);
			if (!document) break;
			add_to_string_list(&document->css_imports, url, -1);
			break;
		}
	}

	return NULL;
}

void
free_table_cache(void)
{
	if (table_cache) {
		struct hash_item *item;
		int i;

		/* We do not free key here. */
		foreach_hash_item (item, *table_cache, i)
			mem_free_if(item->value);

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
			int node_width = !table_level ? MAXINT : width;

			set_rect(node->box, xs, ys, node_width, 1);
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

	done_link_state_info();
	nobreak = 1;

	part = mem_calloc(1, sizeof(struct part));
	if (!part) goto ret;

	part->document = document;
	part->x = xs;
	part->y = ys;
	part->cx = -1;
	part->cy = 0;
	part->link_num = link_num;

	html_state = init_html_parser_state(ELEMENT_IMMORTAL, align, m, width);

	parse_html(start, end, part, head);

	done_html_parser_state(html_state);

	part->max_width = int_max(part->max_width, part->width);

	nobreak = 0;

	done_link_state_info();
	mem_free_if(part->spaces);

	if (document) {
		struct node *node = document->nodes.next;

		node->box.height = ys - node->box.y + part->height;
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

void
render_html_document(struct cache_entry *cached, struct document *document)
{
	struct fragment *fr;
	struct part *part;
	unsigned char *url;
	unsigned char *start = NULL;
	unsigned char *end = NULL;
	struct string title;
	struct string head;
	int i;

	assert(cached && document && !list_empty(cached->frag));
	if_assert_failed return;

	if (!init_string(&head)) return;

	g_ctrl_num = 0;
	url = get_cache_uri_string(cached);

	fr = cached->frag.next;
	start = fr->data;
	end = fr->data + fr->length;

	if (cached->head) add_to_string(&head, cached->head);

	init_html_parser(url, &document->options, start, end, &head, &title,
			 (void (*)(void *, unsigned char *, int)) put_chars_conv,
			 (void (*)(void *)) line_break,
			 (void *(*)(void *, enum html_special_type, ...)) html_special);

	convert_table = get_convert_table(head.source, document->options.cp,
					  document->options.assume_cp,
					  &document->cp,
					  &document->cp_status,
					  document->options.hard_assume);

	document->title = convert_string(convert_table, title.source, title.length, CSM_DEFAULT);
	done_string(&title);

	part = format_html_part(start, end, par_format.align,
			      par_format.leftmargin, document->options.width, document,
			      0, 0, head.source, 1);
	mem_free_if(part);

	done_string(&head);

	document->width = 0;

	for (i = document->height - 1; i >= 0; i--) {
		if (!document->data[i].length) {
			mem_free_if(document->data[i].chars);
			document->height--;
		} else break;
	}

	for (i = 0; i < document->height; i++)
		document->width = int_max(document->width, document->data[i].length);

	/* FIXME: This needs more tuning since if we are centering stuff it
	 * does not work. */
#if 1
	document->options.needs_width = 1;
#else
	document->options.needs_width =
				(document->width + document->options.margin
				 >= document->options.width);
#endif

	document->bgcolor = par_format.bgcolor;

	done_html_parser();

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
