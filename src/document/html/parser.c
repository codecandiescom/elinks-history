/* HTML parser */
/* $Id: parser.c,v 1.222 2003/10/16 13:08:41 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#define _GNU_SOURCE /* strcasestr() */

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "main.h"
#include "bfu/menu.h"
#include "config/options.h"
#include "config/kbdbind.h"
#include "document/html/parser.h"
#include "document/html/renderer.h"
#include "document/html/tables.h"
#include "globhist/globhist.h"
#include "intl/charsets.h"
#include "protocol/http/header.h"
#include "protocol/uri.h"
#include "sched/session.h"
#include "terminal/draw.h"
#include "terminal/terminal.h"
#include "util/color.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/fastfind.h"
#include "util/memdebug.h"
#include "util/memlist.h"
#include "util/memory.h"
#include "util/string.h"
#include "viewer/text/link.h"
#include "viewer/text/view.h"

/* TODO: This needs rewrite. Yes, no kidding. */

INIT_LIST_HEAD(html_stack);

static inline int
atchr(register unsigned char c)
{
	return (c < 127 && (c > '>' || (c > ' ' && c != '=' && c != '<' && c != '>')));
}

/* This function eats one html element. */
/* - e is pointer to the begining of the element (*e must be '<')
 * - eof is pointer to the end of scanned area
 * - parsed element name is stored in name, it's length is namelen
 * - first attribute is stored in attr
 * - end points to first character behind the html element */
/* It returns -1 when it failed (returned values in pointers are invalid) and
 * 0 for success. */
int
parse_element(register unsigned char *e, unsigned char *eof,
	      unsigned char **name, int *namelen,
	      unsigned char **attr, unsigned char **end)
{
	unsigned char saved_eof_char;

	if (e >= eof || *(e++) != '<') return -1;
	if (name) *name = e;

	saved_eof_char = *eof;
	*eof = '\0';

	if (*e == '/') e++;
	if (e >= eof || !isA(*e)) goto parse_error;

	while (isA(*e)) e++;
	if (e >= eof
	    || (!WHITECHAR(*e) && *e != '>' && *e != '<' && *e != '/'
		&& *e != ':'))
		goto parse_error;

	if (name && namelen) *namelen = e - *name;

	while (WHITECHAR(*e) || *e == '/' || *e == ':') e++;
	if (e >= eof || (!atchr(*e) && *e != '>' && *e != '<')) goto parse_error;

	if (attr) *attr = e;

next_attr:
	while (WHITECHAR(*e)) e++;
	if (e >= eof || (!atchr(*e) && *e != '>' && *e != '<')) goto parse_error;

	if (*e == '>' || *e == '<') goto end;

	while (atchr(*e)) e++;
	while (WHITECHAR(*e)) e++;
	if (e >= eof) goto parse_error;

	if (*e != '=') {
		if (*e == '>' || *e == '<') goto end;
		goto next_attr;
	}

	e++;

	while (WHITECHAR(*e)) e++;
	if (e >= eof) goto parse_error;

	if (IS_QUOTE(*e)) {
		unsigned char quote = *e;

quoted_value:
		e++;
		while (*e != quote && *e) e++;
		if (e >= eof || *e < ' ') goto parse_error;
		e++;
		if (e >= eof) goto parse_error;
		if (*e == quote) goto quoted_value;
	} else {
		while (*e && !WHITECHAR(*e) && *e != '>' && *e != '<') e++;
		if (e >= eof) goto parse_error;
	}

	while (WHITECHAR(*e)) e++;
	if (e >= eof) goto parse_error;

	if (*e != '>' && *e != '<') goto next_attr;

end:
	if (end) *end = e + (*e == '>');
	*eof = saved_eof_char;
	return 0;

parse_error:
	*eof = saved_eof_char;
	return -1;
}

#define realloc_chrs(x, l) \
	mem_align_alloc(x, l, (l) + 1, sizeof(unsigned char), 0xFF)

#define add_chr(s, l, c)						\
	do {								\
		if (!realloc_chrs(&(s), l)) return NULL;		\
		(s)[(l)++] = (c);					\
	} while (0)


/* Eat newlines when loading attribute value? */
static int get_attr_val_eat_nl = 0;

/* Parses html element attributes. */
/* - e is attr pointer previously get from parse_element,
 * DON'T PASS HERE ANY OTHER VALUE!!!
 * - name is searched attribute */
/* Returns allocated string containing the attribute, or NULL on unsuccess. */
unsigned char *
get_attr_val(register unsigned char *e, unsigned char *name)
{
	unsigned char *n;
	unsigned char *attr = NULL;
	int attrlen = 0;
	int found;

next_attr:
	while (WHITECHAR(*e)) e++;
	if (*e == '>' || *e == '<') return NULL;
	n = name;

	while (*n && upcase(*e) == upcase(*n)) e++, n++;
	found = !*n && !atchr(*e);

	while (atchr(*e)) e++;
	while (WHITECHAR(*e)) e++;
	if (*e != '=') {
		if (found) goto found_endattr;
		goto next_attr;
	}
	e++;
	while (WHITECHAR(*e)) e++;

	if (found) {
		if (!IS_QUOTE(*e)) {
			while (!WHITECHAR(*e) && *e != '>' && *e != '<') {
				add_chr(attr, attrlen, *e);
				e++;
			}
		} else {
			unsigned char quote = *e;

parse_quoted_value:
			while (*(++e) != quote) {
				if (*e == ASCII_CR) continue;
				if (!*e) goto end;
				if (*e != ASCII_TAB && *e != ASCII_LF)
					add_chr(attr, attrlen, *e);
				else if (!get_attr_val_eat_nl)
					add_chr(attr, attrlen, ' ');
			}
			e++;
			if (*e == quote) {
				add_chr(attr, attrlen, *e);
				goto parse_quoted_value;
			}
		}

found_endattr:
		add_chr(attr, attrlen, 0);
		attrlen--;
		if (memchr(attr, '&', attrlen)) {
			unsigned char *saved_attr = attr;

			attr = convert_string(NULL, saved_attr, attrlen, CSM_QUERY);
			mem_free(saved_attr);
		}

		set_mem_comment(trim_chars(attr, ' ', NULL), name, strlen(name));
		return attr;

	} else {
		if (!IS_QUOTE(*e)) {
			while (!WHITECHAR(*e) && *e != '>' && *e != '<') e++;
		} else {
			unsigned char quote = *e;

			do {
				while (*(++e) != quote)
					if (!*e) goto end;
				e++;
			} while (*e == quote);
		}
	}

	goto next_attr;

end:
	if (attr) mem_free(attr);
	return NULL;
}

#undef add_chr


static inline unsigned char *
get_url_val(unsigned char *e, unsigned char *name)
{
	unsigned char *val;

	get_attr_val_eat_nl = 1;
	val = get_attr_val(e, name);
	get_attr_val_eat_nl = 0;
	return val;
}

int
has_attr(unsigned char *e, unsigned char *name)
{
	unsigned char *a = get_attr_val(e, name);

	if (!a) return 0;
	mem_free(a);

	return 1;
}

static struct {
	int n;
	unsigned char *s;
} roman_tbl[] = {
	{1000,	"m"},
	{999,	"im"},
	{990,	"xm"},
	{900,	"cm"},
	{500,	"d"},
	{499,	"id"},
	{490,	"xd"},
	{400,	"cd"},
	{100,	"c"},
	{99,	"ic"},
	{90,	"xc"},
	{50,	"l"},
	{49,	"il"},
	{40,	"xl"},
	{10,	"x"},
	{9,	"ix"},
	{5,	"v"},
	{4,	"iv"},
	{1,	"i"},
	{0,	NULL}
};

static void
roman(unsigned char *p, unsigned n)
{
	int i = 0;

	if (n >= 4000) {
		strcpy(p, "---");
		return;
	}
	if (!n) {
		strcpy(p, "o");
		return;
	}
	p[0] = 0;
	while (n) {
		while (roman_tbl[i].n <= n) {
			n -= roman_tbl[i].n;
			strcat(p, roman_tbl[i].s);
		}
		i++;
		assertm(!(n && !roman_tbl[i].n),
			"BUG in roman number convertor");
		if_assert_failed break;
	}
}

static int
get_color(unsigned char *a, unsigned char *c, color_t *rgb)
{
	unsigned char *at;
	int r;

	if (!d_opt->color_mode || d_opt->use_document_colours < 1)
		return -1;

	at = get_attr_val(a, c);
	if (!at) return -1;

	r = decode_color(at, rgb);
	mem_free(at);

	return r;
}

int
get_bgcolor(unsigned char *a, color_t *rgb)
{
	if (!d_opt->color_mode || d_opt->use_document_colours < 2)
		return -1;
	return get_color(a, "bgcolor", rgb);
}

static unsigned char *
get_target(unsigned char *a)
{
	unsigned char *v = get_attr_val(a, "target");

	if (v) {
		if (!strcasecmp(v, "_self")) {
			mem_free(v);
			v = stracpy(d_opt->framename);
		}
	}

	return v;
}

void
kill_html_stack_item(struct html_element *e)
{
	assert(e);
	if_assert_failed return;
	assertm((void *)e != &html_stack, "trying to free bad html element");
	if_assert_failed return;
	assertm(e->dontkill != 2, "trying to kill unkillable element");
	if_assert_failed return;

	if (e->attr.link) mem_free(e->attr.link);
	if (e->attr.target) mem_free(e->attr.target);
	if (e->attr.image) mem_free(e->attr.image);
	if (e->attr.title) mem_free(e->attr.title);
	if (e->attr.href_base) mem_free(e->attr.href_base);
	if (e->attr.target_base) mem_free(e->attr.target_base);
	if (e->attr.select) mem_free(e->attr.select);
	del_from_list(e);
	mem_free(e);
#if 0
	if (list_empty(html_stack) || !html_stack.next) {
		debug("killing last element");
	}
#endif
}

static inline void
kill_elem(unsigned char *e)
{
	if (html_top.namelen == strlen(e)
	    && !strncasecmp(html_top.name, e, html_top.namelen))
		kill_html_stack_item(&html_top);
}

#if 0
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
/* Never called */
void
debug_stack(void)
{
	struct html_element *e;

	printf("HTML stack debug: \n");
	foreachback (e, html_stack) {
		int i;

		printf("\"");
		for (i = 0; i < e->namelen; i++) printf("%c", e->name[i]);
		printf("\"\n");
	}
	printf("%c", 7);
	fflush(stdout);
	sleep(1);
}
#endif

void
html_stack_dup(void)
{
	struct html_element *e;
	struct html_element *ep = html_stack.next;

	assertm(ep && (void *)ep != &html_stack, "html stack empty");
	if_assert_failed return;

	e = mem_alloc(sizeof(struct html_element));
	if (!e) return;
	memcpy(e, ep, sizeof(struct html_element));
	if (ep->attr.link) copy_string(&e->attr.link, ep->attr.link);
	if (ep->attr.target) copy_string(&e->attr.target, ep->attr.target);
	if (ep->attr.image) copy_string(&e->attr.image, ep->attr.image);
	if (ep->attr.title) copy_string(&e->attr.title, ep->attr.title);
	if (ep->attr.href_base) copy_string(&e->attr.href_base, ep->attr.href_base);
	if (ep->attr.target_base) copy_string(&e->attr.target_base, ep->attr.target_base);
	if (ep->attr.select) copy_string(&e->attr.select, ep->attr.select);
#if 0
	if (e->name) {
		if (e->attr.link) set_mem_comment(e->attr.link, e->name, e->namelen);
		if (e->attr.target) set_mem_comment(e->attr.target, e->name, e->namelen);
		if (e->attr.image) set_mem_comment(e->attr.image, e->name, e->namelen);
		if (e->attr.title) set_mem_comment(e->attr.title, e->name, e->namelen);
		if (e->attr.href_base) set_mem_comment(e->attr.href_base, e->name, e->namelen);
		if (e->attr.target_base) set_mem_comment(e->attr.target_base, e->name, e->namelen);
		if (e->attr.select) set_mem_comment(e->attr.select, e->name, e->namelen);
	}
#endif
	e->name = e->options = NULL;
	e->namelen = 0;
	e->dontkill = 0;
	add_to_list(html_stack, e);
}

void *ff;
void (*put_chars_f)(void *, unsigned char *, int);
void (*line_break_f)(void *);
void (*init_f)(void *);
void *(*special_f)(void *, int, ...);

unsigned char *eoff;
unsigned char *eofff;
unsigned char *startf;

int line_breax;
static int position;
static int putsp;

static int was_br;
static int was_li;

static inline void
ln_break(int n, void (*line_break)(void *), void *f)
{
	if (!n || html_top.invisible) return;
	while (n > line_breax) line_breax++, line_break(f);
	position = 0;
	putsp = -1;
}

static void
put_chrs(unsigned char *start, int len,
	 void (*put_chars)(void *, unsigned char *, int), void *f)
{
	if (par_format.align == AL_NONE) putsp = 0;
	if (!len || html_top.invisible) return;
	if (putsp == 1) put_chars(f, " ", 1), position++, putsp = -1;
	if (putsp == -1) {
		if (start[0] == ' ') start++, len--;
		putsp = 0;
	}
	if (!len) {
		putsp = -1;
		if (par_format.align == AL_NONE) putsp = 0;
		return;
	}
	if (start[len - 1] == ' ') putsp = -1;
	if (par_format.align == AL_NONE) putsp = 0;
	was_br = 0;
	put_chars(f, start, len);
	position += len;
	line_breax = 0;
	if (was_li > 0) was_li--;
}

static void
kill_until(int ls, ...)
{
	int l;
	struct html_element *e = &html_top;

	if (ls) e = e->next;

	while ((void *)e != &html_stack) {
		int sk = 0;
		va_list arg;

		va_start(arg, ls);
		while (1) {
			unsigned char *s = va_arg(arg, unsigned char *);

			if (!s) break;
			if (!*s) {
				sk++;
			} else {
				int slen = strlen(s);

				if (e->namelen == slen
				    && !strncasecmp(e->name, s, slen)) {
					if (!sk) {
						if (e->dontkill) break;
						va_end(arg);
						goto killll;
					} else if (sk == 1) {
						va_end(arg);
						goto killl;
					} else {
						break;
					}
				}
			}
		}
		va_end(arg);
		if (e->dontkill || (e->namelen == 5 && !strncasecmp(e->name, "TABLE", 5))) break;
		if (e->namelen == 2 && upcase(e->name[0]) == 'T') {
			unsigned char c = upcase(e->name[1]);

			if (c == 'D' || c == 'H' || c == 'R') break;
		}
		e = e->next;
	}
	return;

killl:
	e = e->prev;
killll:
	l = 0;
	while ((void *)e != &html_stack) {
		if (ls && e == html_stack.next) break;
		if (e->linebreak > l) l = e->linebreak;
		e = e->prev;
		kill_html_stack_item(e->next);
	}
	ln_break(l, line_break_f, ff);
}

int
get_num(unsigned char *a, unsigned char *n)
{
	unsigned char *al = get_attr_val(a, n);

	if (al) {
		unsigned char *end;
		int s;

		errno = 0;
		s = strtoul(al, (char **)&end, 10);
		if (errno || !*al || *end || s < 0) s = -1;
		mem_free(al);

		return s;
	}

	return -1;
}

static int
parse_width(unsigned char *w, int trunc)
{
	unsigned char *end;
	int p = 0;
	int s;
	int l;
	int width;

	while (WHITECHAR(*w)) w++;
	for (l = 0; w[l] && w[l] != ','; l++);

	while (l && WHITECHAR(w[l - 1])) l--;
	if (!l) return -1;

	if (w[l - 1] == '%') l--, p = 1;

	while (l && WHITECHAR(w[l - 1])) l--;
	if (!l) return -1;

	width = par_format.width - par_format.leftmargin - par_format.rightmargin;

	errno = 0;
	s = strtoul((char *)w, (char **)&end, 10);
	if (errno) return -1;

	if (p) {
		if (trunc)
			s = s * width / 100;
		else
			return -1;
	} else s = (s + (HTML_CHAR_WIDTH - 1) / 2) / HTML_CHAR_WIDTH;

	if (trunc && s > width) s = width;

	if (s < 0) s = 0;

	return s;
}

int
get_width(unsigned char *a, unsigned char *n, int trunc)
{
	int r;
	unsigned char *w = get_attr_val(a, n);

	if (!w) return -1;
	r = parse_width(w, trunc);
	mem_free(w);

	return r;
}

struct form form = NULL_STRUCT_FORM;

unsigned char *last_form_tag;
unsigned char *last_form_attr;
unsigned char *last_input_tag;

static void
put_link_line(unsigned char *prefix, unsigned char *linkname,
	      unsigned char *link, unsigned char *target)
{
	html_stack_dup();
	ln_break(1, line_break_f, ff);
	if (format.link) mem_free(format.link),	format.link = NULL;
	if (format.target) mem_free(format.target), format.target = NULL;
	if (format.title) mem_free(format.title), format.title = NULL;
	format.form = NULL;
	put_chrs(prefix, strlen(prefix), put_chars_f, ff);
	format.link = join_urls(format.href_base, link);
	format.target = stracpy(target);
	format.fg = format.clink;
	put_chrs(linkname, strlen(linkname), put_chars_f, ff);
	ln_break(1, line_break_f, ff);
	kill_html_stack_item(&html_top);
}

static inline void
html_span(unsigned char *a)
{
}

static inline void
html_bold(unsigned char *a)
{
	format.attr |= AT_BOLD;
}

static inline void
html_italic(unsigned char *a)
{
	format.attr |= AT_ITALIC;
}

static inline void
html_underline(unsigned char *a)
{
	format.attr |= AT_UNDERLINE;
}

static inline void
html_fixed(unsigned char *a)
{
	format.attr |= AT_FIXED;
}

static inline void
html_subscript(unsigned char *a)
{
	format.attr |= AT_SUBSCRIPT;
}

static inline void
html_superscript(unsigned char *a)
{
	format.attr |= AT_SUPERSCRIPT;
}

/* Extract the extra information that is available for elements which can
 * receive focus. Call this from each element which supports tabindex or
 * accesskey. */
/* Note that in ELinks, we support those attributes (I mean, we call this
 * function) while processing any focusable element (otherwise it'd have zero
 * tabindex, thus messing up navigation between links), thus we support these
 * attributes even near tags where we're not supposed to (like IFRAME, FRAME or
 * LINK). I think this doesn't make any harm ;). --pasky */
static void
html_focusable(unsigned char *a)
{
	unsigned char *accesskey;
	int tabindex;

	format.accesskey = 0;
	format.tabindex = 0x80000000;

	if (!a) return;

	accesskey = get_attr_val(a, "accesskey");
	if (accesskey) {
		accesskey[0] = upcase(accesskey[0]);
		format.accesskey = read_key(accesskey);
		mem_free(accesskey);
	}

	tabindex = get_num(a, "tabindex");
	if (tabindex > 0) {
		format.tabindex = (tabindex & 0x7fff) << 16;
	}
}

static void
html_a(unsigned char *a)
{
	unsigned char *href, *name;

	href = get_url_val(a, "href");
	if (href) {
		unsigned char *target;

		if (format.link) mem_free(format.link);
		format.link = join_urls(format.href_base, trim_chars(href, ' ', 0));

		mem_free(href);

		target = get_target(a);
		if (target) {
			if (format.target) mem_free(format.target);
			format.target = target;
		} else {
			if (format.target) mem_free(format.target);
			format.target = stracpy(format.target_base);
		}
#ifdef GLOBHIST
		if (get_global_history_item(format.link))
			format.fg = format.vlink;
		else
#endif
			format.fg = format.clink;

		if (format.title) mem_free(format.title);
		format.title = get_attr_val(a, "title");

		html_focusable(a);

	} else {
		kill_html_stack_item(&html_top);
	}

	name = get_attr_val(a, "name");
	if (name) {
		special_f(ff, SP_TAG, name);
		mem_free(name);
	}
}

static void
html_font(unsigned char *a)
{
	unsigned char *al = get_attr_val(a, "size");

	if (al) {
		int p = 0;
		unsigned s;
		unsigned char *nn = al;
		unsigned char *end;

		if (*al == '+') p = 1, nn++;
		else if (*al == '-') p = -1, nn++;

		errno = 0;
		s = strtoul(nn, (char **)&end, 10);
		if (!errno && *nn && !*end) {
			if (s > 7) s = 7;
			if (!p) format.fontsize = s;
			else format.fontsize += p * s;
			if (format.fontsize < 1) format.fontsize = 1;
			else if (format.fontsize > 7) format.fontsize = 7;
		}
		mem_free(al);
	}
	get_color(a, "color", &format.fg);
}

static void
html_img(unsigned char *a)
{
	int ismap, usemap = 0;
	int add_brackets = 0;
	unsigned char *al = get_attr_val(a, "usemap");

	if (al) {
		unsigned char *u;

		usemap = 1;
		html_stack_dup();
		if (format.link) mem_free(format.link);
		if (format.form) format.form = NULL;
		u = join_urls(format.href_base, al);
		if (!u) {
			mem_free(al);
			return;
		}

		format.link = straconcat("MAP@", u, NULL);
			format.attr |= AT_BOLD;
		mem_free(u);
		mem_free(al);
	}
	ismap = format.link && has_attr(a, "ismap") && !usemap;

	al = get_attr_val(a, "alt");
	if (!al) al = get_attr_val(a, "title");

	if (!al || !*al) {
		if (al) mem_free(al);
		if (!d_opt->images && !format.link) return;

		add_brackets = 1;

		if (usemap) {
			al = stracpy("USEMAP");
		} else if (ismap) {
			al = stracpy("ISMAP");
		} else {
			unsigned char *src = get_url_val(a, "src");
			int max_real_len;
			int max_len;

			/* We can display image as [foo.gif]. */

			max_len = get_opt_int("document.browse.images.file_tags");

#if 0
			/* This should be maybe whole terminal width? */
			max_real_len = par_format.width * max_len / 100;
#else
			/* It didn't work well and I'm too lazy to code that;
			 * absolute values will have to be enough for now ;).
			 * --pasky */
			max_real_len = max_len;
#endif

			if ((!max_len || max_real_len > 0) && src) {
				int len = strcspn(src, "?");
				unsigned char *start;

				for (start = src + len; start > src; start--)
					if (dir_sep(*start)) {
						start++;
						break;
					}

				if (start > src) len = strcspn(start, "?");

				if (max_len && len > max_real_len) {
					int max_part_len = max_real_len / 2;

					al = mem_alloc(max_part_len * 2 + 2);
					if (!al) return;

					/* TODO: Faster way ?? sprintf() is quite expensive. */
					sprintf(al, "%.*s*%.*s",
						max_part_len, start,
						max_part_len, start + len
							      - max_part_len);

				} else {
					al = mem_alloc(len + 1);
					if (!al) return;

					/* TODO: Faster way ?? */
					sprintf(al, "%.*s", len, start);
				}
			} else {
				al = stracpy("IMG");
			}

			if (src) mem_free(src);
		}
	}

	if (format.image) mem_free(format.image), format.image = NULL;
	if (format.title) mem_free(format.title), format.title = NULL;

	if (al) {
		int img_link_tag = get_opt_int("document.browse.images.image_link_tagging");
		unsigned char *s;

		if (img_link_tag && (img_link_tag == 2 || add_brackets)) {
			unsigned char *img_link_prefix = get_opt_str("document.browse.images.image_link_prefix");
			unsigned char *img_link_suffix = get_opt_str("document.browse.images.image_link_suffix");
			unsigned char *tmp = straconcat(img_link_prefix, al, img_link_suffix, NULL);

			if (tmp) {
				mem_free(al);
				al = tmp;
			}
		}

		/* This is not 100% appropriate for <img>, but well, accepting
		 * accesskey and tabindex near <img> is just our little
		 * extension to the standart. After all, it makes sense. */
		html_focusable(a);

		if ((s = get_url_val(a, "src")) || (s = get_url_val(a, "dynsrc"))) {
			format.image = join_urls(format.href_base, s);
			mem_free(s);
		}

		format.title = get_attr_val(a, "title");

		if (ismap) {
			unsigned char *h;

			html_stack_dup();
			h = stracpy(format.link);
			if (h) {
				add_to_strn(&h, "?0,0");
				mem_free(format.link);
				format.link = h;
			}
		}
		put_chrs(al, strlen(al), put_chars_f, ff);
		if (ismap) kill_html_stack_item(&html_top);
	}
	if (format.image) mem_free(format.image), format.image = NULL;
	if (format.title) mem_free(format.title), format.title = NULL;
	if (al) mem_free(al);
	if (usemap) kill_html_stack_item(&html_top);
	/*put_chrs(" ", 1, put_chars_f, ff);*/
}

static void
html_body(unsigned char *a)
{
	get_color(a, "text", &format.fg);
	get_color(a, "link", &format.clink);
	get_color(a, "vlink", &format.vlink);
	get_bgcolor(a, &par_format.bgcolor);
	if (get_bgcolor(a, &format.bg) >= 0) {
		/* Modify the root HTML element - format_html_part() will take
		 * this from there. */
		((struct html_element *) html_stack.prev)->parattr.bgcolor = par_format.bgcolor;
		((struct html_element *) html_stack.prev)->attr.bg = format.bg;
	}
	special_f(ff, SP_COLOR_LINK_LINES);
}

static void
html_skip(unsigned char *a)
{
	html_top.invisible = html_top.dontkill = 1;
}

static void
html_title(unsigned char *a)
{
	html_top.invisible = html_top.dontkill = 1;
}

static void
html_center(unsigned char *a)
{
	par_format.align = AL_CENTER;
	if (!table_level) par_format.leftmargin = par_format.rightmargin = 0;
}

static void
html_linebrk(unsigned char *a)
{
	unsigned char *al = get_attr_val(a, "align");

	if (al) {
		if (!strcasecmp(al, "left")) par_format.align = AL_LEFT;
		else if (!strcasecmp(al, "right")) par_format.align = AL_RIGHT;
		else if (!strcasecmp(al, "center")) {
			par_format.align = AL_CENTER;
			if (!table_level) par_format.leftmargin = par_format.rightmargin = 0;
		}
		else if (!strcasecmp(al, "justify")) par_format.align = AL_BLOCK;
		mem_free(al);
	}
}

static void
html_br(unsigned char *a)
{
	html_linebrk(a);
	if (was_br)
		ln_break(2, line_break_f, ff);
	else
		was_br = 1;
}

static void
html_form(unsigned char *a)
{
	was_br = 1;
}

static void
html_p(unsigned char *a)
{
	int_lower_bound(&par_format.leftmargin, margin);
	int_lower_bound(&par_format.rightmargin, margin);
	/*par_format.align = AL_LEFT;*/
	html_linebrk(a);
}

static void
html_address(unsigned char *a)
{
	par_format.leftmargin++;
	par_format.align = AL_LEFT;
}

static void
html_blockquote(unsigned char *a)
{
	par_format.leftmargin += 2;
	par_format.align = AL_LEFT;
}

static void
html_h(int h, unsigned char *a,
       enum format_align default_align)
{
	par_format.align = default_align;
	html_linebrk(a);

	h -= 2;
	if (h < 0) h = 0;

	switch (par_format.align) {
		case AL_LEFT:
			par_format.leftmargin = h << 1;
			par_format.rightmargin = 0;
			break;
		case AL_RIGHT:
			par_format.leftmargin = 0;
			par_format.rightmargin = h << 1;
			break;
		case AL_CENTER:
			par_format.leftmargin = par_format.rightmargin = 0;
			break;
		case AL_BLOCK:
			par_format.leftmargin = par_format.rightmargin = h << 1;
			break;
		case AL_NONE:
			/* Silence compiler warnings */
			break;
	}
}

static void
html_h1(unsigned char *a)
{
	format.attr |= AT_BOLD;
	html_h(1, a, AL_CENTER);
}

static void
html_h2(unsigned char *a)
{
	html_h(2, a, AL_LEFT);
}

static void
html_h3(unsigned char *a)
{
	html_h(3, a, AL_LEFT);
}

static void
html_h4(unsigned char *a)
{
	html_h(4, a, AL_LEFT);
}

static void
html_h5(unsigned char *a)
{
	html_h(5, a, AL_LEFT);
}

static void
html_h6(unsigned char *a)
{
	html_h(6, a, AL_LEFT);
}

static void
html_pre(unsigned char *a)
{
	par_format.align = AL_NONE;
	par_format.leftmargin = par_format.leftmargin > 1;
	par_format.rightmargin = 0;
}

static void
html_xmp(unsigned char *a)
{
	d_opt->plain = 2;
	html_pre(a);
}

static void
html_hr(unsigned char *a)
{
	int i/* = par_format.width - 10*/;
	unsigned char r = (unsigned char)BORDER_DHLINE;
	int q = get_num(a, "size");

	if (q >= 0 && q < 2) r = (unsigned char)BORDER_SHLINE;
	html_stack_dup();
	par_format.align = AL_CENTER;
	if (format.link) mem_free(format.link), format.link = NULL;
	format.form = NULL;
	html_linebrk(a);
	if (par_format.align == AL_BLOCK) par_format.align = AL_CENTER;
	par_format.leftmargin = par_format.rightmargin = margin;

	i = get_width(a, "width", 1);
	if (i == -1) i = par_format.width - (margin - 2) * 2;
	format.attr = AT_GRAPHICS;
	special_f(ff, SP_NOWRAP, 1);
	while (i-- > 0) put_chrs(&r, 1, put_chars_f, ff);
	special_f(ff, SP_NOWRAP, 0);
	ln_break(2, line_break_f, ff);
	kill_html_stack_item(&html_top);
}

static void
html_table(unsigned char *a)
{
	par_format.leftmargin = margin;
	par_format.rightmargin = margin;
	par_format.align = AL_LEFT;
	html_linebrk(a);
	format.attr = 0;
}

static void
html_tr(unsigned char *a)
{
	html_linebrk(a);
}

static void
html_th(unsigned char *a)
{
	/*html_linebrk(a);*/
	kill_until(1, "TD", "TH", "", "TR", "TABLE", NULL);
	format.attr |= AT_BOLD;
	put_chrs(" ", 1, put_chars_f, ff);
}

static void
html_td(unsigned char *a)
{
	/*html_linebrk(a);*/
	kill_until(1, "TD", "TH", "", "TR", "TABLE", NULL);
	format.attr &= ~AT_BOLD;
	put_chrs(" ", 1, put_chars_f, ff);
}

static void
html_base(unsigned char *a)
{
	unsigned char *al = get_url_val(a, "href");

	if (al) {
		if (format.href_base) mem_free(format.href_base);
		format.href_base = join_urls(((struct html_element *)html_stack.prev)->attr.href_base, al);
		mem_free(al);
	}

	al = get_target(a);
	if (al) {
		if (format.target_base) mem_free(format.target_base);
		format.target_base = al;
	}
}

static void
html_ul(unsigned char *a)
{
	unsigned char *al;

	/*debug_stack();*/
	par_format.list_level++;
	par_format.list_number = 0;
	par_format.flags = P_STAR;

	al = get_attr_val(a, "type");
	if (al) {
		if (!strcasecmp(al, "disc") || !strcasecmp(al, "circle"))
			par_format.flags = P_O;
		else if (!strcasecmp(al, "square"))
			par_format.flags = P_PLUS;
		mem_free(al);
	}
	par_format.leftmargin += 2 + (par_format.list_level > 1);
	if (!table_level && par_format.leftmargin > par_format.width / 2)
		par_format.leftmargin = par_format.width / 2;
	par_format.align = AL_LEFT;
	html_top.dontkill = 1;
}

static void
html_ol(unsigned char *a)
{
	unsigned char *al;
	int st;

	par_format.list_level++;
	st = get_num(a, "start");
	if (st == -1) st = 1;
	par_format.list_number = st;
	par_format.flags = P_NUMBER;

	al = get_attr_val(a, "type");
	if (al) {
		if (*al && !al[1]) {
			if (*al == '1') par_format.flags = P_NUMBER;
			else if (*al == 'a') par_format.flags = P_alpha;
			else if (*al == 'A') par_format.flags = P_ALPHA;
			else if (*al == 'r') par_format.flags = P_roman;
			else if (*al == 'R') par_format.flags = P_ROMAN;
			else if (*al == 'i') par_format.flags = P_roman;
			else if (*al == 'I') par_format.flags = P_ROMAN;
		}
		mem_free(al);
	}

	par_format.leftmargin += (par_format.list_level > 1);
	if (!table_level)
		int_upper_bound(&par_format.leftmargin, par_format.width / 2);

	par_format.align = AL_LEFT;
	html_top.dontkill = 1;
}

static void
html_li(unsigned char *a)
{
	/* When handling the code <li><li> @was_li will be 1 and it means we
	 * have to insert a line break since no list item content has done it
	 * for us. */
	if (was_li) {
		line_breax = 0;
		ln_break(1, line_break_f, ff);
	}

	/*kill_until(0, "", "UL", "OL", NULL);*/
	if (!par_format.list_number) {
		unsigned char x[7] = "*&nbsp;";

		if ((par_format.flags & P_LISTMASK) == P_O) x[0] = 'o';
		if ((par_format.flags & P_LISTMASK) == P_PLUS) x[0] = '+';
		put_chrs(x, 7, put_chars_f, ff);
		par_format.leftmargin += 2;
		par_format.align = AL_LEFT;
	} else {
		unsigned char c = 0;
		unsigned char n[32];
		int t = par_format.flags & P_LISTMASK;
		int s = get_num(a, "value");

		if (s != -1) par_format.list_number = s;
		if ((t != P_roman && t != P_ROMAN && par_format.list_number < 10)
		    || t == P_alpha || t == P_ALPHA)
			put_chrs("&nbsp;", 6, put_chars_f, ff), c = 1;

		if (t == P_ALPHA || t == P_alpha) {
			n[0] = par_format.list_number
			       ? (par_format.list_number - 1) % 26
			         + (t == P_ALPHA ? 'A' : 'a')
			       : 0;
			n[1] = 0;
		} else if (t == P_ROMAN || t == P_roman) {
			roman(n, par_format.list_number);
			if (t == P_ROMAN) {
				register unsigned char *x;

				for (x = n; *x; x++) *x = upcase(*x);
			}
		} else {
			ulongcat(n, NULL, par_format.list_number, (sizeof(n) - 1), 0);
		}
		put_chrs(n, strlen(n), put_chars_f, ff);
		put_chrs(".&nbsp;", 7, put_chars_f, ff);
		par_format.leftmargin += strlen(n) + c + 2;
		par_format.align = AL_LEFT;
		par_format.list_number = 0;
		html_top.next->parattr.list_number++;
	}

	putsp = -1;
	line_breax = 2;
	was_li = 1;
}

static void
html_dl(unsigned char *a)
{
	par_format.flags &= ~P_COMPACT;
	if (has_attr(a, "compact")) par_format.flags |= P_COMPACT;
	if (par_format.list_level) par_format.leftmargin += 5;
	par_format.list_level++;
	par_format.list_number = 0;
	par_format.align = AL_LEFT;
	par_format.dd_margin = par_format.leftmargin;
	html_top.dontkill = 1;
	if (!(par_format.flags & P_COMPACT)) {
		ln_break(2, line_break_f, ff);
		html_top.linebreak = 2;
	}
}

static void
html_dt(unsigned char *a)
{
	kill_until(0, "", "DL", NULL);
	par_format.align = AL_LEFT;
	par_format.leftmargin = par_format.dd_margin;
	if (!(par_format.flags & P_COMPACT) && !has_attr(a, "compact"))
		ln_break(2, line_break_f, ff);
}

static void
html_dd(unsigned char *a)
{
	kill_until(0, "", "DL", NULL);

	par_format.leftmargin = par_format.dd_margin + (table_level ? 3 : 8);
	if (!table_level)
		int_upper_bound(&par_format.leftmargin, par_format.width / 2);
	par_format.align = AL_LEFT;
}

static void
get_html_form(unsigned char *a, struct form *form)
{
	unsigned char *al;

	form->method = FM_GET;

	al = get_attr_val(a, "method");
	if (al) {
		if (!strcasecmp(al, "post")) {
			char *ax = get_attr_val(a, "enctype");

			form->method = FM_POST;
			if (ax) {
				if (!strcasecmp(ax, "multipart/form-data"))
					form->method = FM_POST_MP;
				mem_free(ax);
			}
		}
		mem_free(al);
	}

	al = get_attr_val(a, "action");
	if (al) {
		form->action = join_urls(format.href_base, trim_chars(al, ' ', 0));
		mem_free(al);
	} else {
		form->action = stracpy(format.href_base);
		if (form->action) {
			unsigned char *ch = strchr(form->action, POST_CHAR);
			if (ch) *ch = 0;

			/* We have to do following for GET method, because we would end
			 * up with two '?' otherwise. */
			if (form->method == FM_GET) {
				ch = strchr(form->action, '?');
				if (ch) *ch = 0;
			}
		}
	}

	al = get_target(a);
	if (al) {
		form->target = al;
	} else {
		form->target = stracpy(format.target_base);
	}

	form->num = a - startf;
}

static void
find_form_for_input(unsigned char *i)
{
	unsigned char *s, *ss, *name, *attr;
	unsigned char *lf = NULL;
	unsigned char *la = NULL;
	int namelen;

	if (form.action) mem_free(form.action);
	if (form.target) mem_free(form.target);
	memset(&form, 0, sizeof(form));

	if (!special_f(ff, SP_USED, NULL)) return;

	if (last_input_tag && i <= last_input_tag && i > last_form_tag) {
		get_html_form(last_form_attr, &form);
		return;
	}
	if (last_input_tag && i > last_input_tag)
		s = last_form_tag;
	else
		s = startf;

se:
	while (s < i && *s != '<') {

sp:
		s++;
	}
	if (s >= i) goto end_parse;
	if (s + 2 <= eofff && (s[1] == '!' || s[1] == '?')) {
		s = skip_comment(s, i);
		goto se;
	}
	ss = s;
	if (parse_element(s, i, &name, &namelen, &attr, &s)) goto sp;
	if (namelen != 4 || strncasecmp(name, "FORM", 4)) goto se;
	lf = ss;
	la = attr;
	goto se;


end_parse:
	if (lf && la) {
		last_form_tag = lf;
		last_form_attr = la;
		last_input_tag = i;
		get_html_form(la, &form);
	} else {
		memset(&form, 0, sizeof(struct form));
	}
}

static void
html_button(unsigned char *a)
{
	unsigned char *al;
	struct form_control *fc;

	find_form_for_input(a);
	html_focusable(a);

	fc = mem_calloc(1, sizeof(struct form_control));
	if (!fc) return;

	al = get_attr_val(a, "type");
	if (!al) {
		fc->type = FC_SUBMIT;
		goto xxx;
	}

	if (!strcasecmp(al, "submit")) fc->type = FC_SUBMIT;
	else if (!strcasecmp(al, "reset")) fc->type = FC_RESET;
	else if (!strcasecmp(al, "button")) {
		mem_free(al);
		put_chrs(" [&nbsp;", 8, put_chars_f, ff);

		al = get_attr_val(a, "value");
		if (al) {
			put_chrs(al, strlen(al), put_chars_f, ff);
			mem_free(al);
		} else put_chrs("BUTTON", 6, put_chars_f, ff);

		put_chrs("&nbsp;] ", 8, put_chars_f, ff);
		mem_free(fc);
		return;
	} else {
		mem_free(al);
		mem_free(fc);
		return;
	}
	mem_free(al);

xxx:
	fc->form_num = last_form_tag - startf;
	fc->ctrl_num = a - last_form_tag;
	fc->position = a - startf;
	fc->method = form.method;
	fc->action = form.action ? stracpy(form.action) : NULL;
	fc->name = get_attr_val(a, "name");
	fc->default_value = get_attr_val(a, "value");
	fc->ro = has_attr(a, "disabled") ? 2 : has_attr(a, "readonly") ? 1 : 0;
	if (fc->type == FC_IMAGE) fc->alt = get_attr_val(a, "alt");
	if (fc->type == FC_SUBMIT && !fc->default_value) fc->default_value = stracpy("Submit");
	if (fc->type == FC_RESET && !fc->default_value) fc->default_value = stracpy("Reset");
	if (!fc->default_value) fc->default_value = stracpy("");
	special_f(ff, SP_CONTROL, fc);
	format.form = fc;
	format.attr |= AT_BOLD;
#if 0
	put_chrs("[&nbsp;", 7, put_chars_f, ff);
	if (fc->default_value) put_chrs(fc->default_value, strlen(fc->default_value), put_chars_f, ff);
	put_chrs("&nbsp;]", 7, put_chars_f, ff);
	put_chrs(" ", 1, put_chars_f, ff);
#endif
}

static void
html_input(unsigned char *a)
{
	int i;
	unsigned char *al;
	struct form_control *fc;

	find_form_for_input(a);
	html_focusable(a);

	fc = mem_calloc(1, sizeof(struct form_control));
	if (!fc) return;

	al = get_attr_val(a, "type");
	if (!al) {
		fc->type = FC_TEXT;
		goto xxx;
	}
	if (!strcasecmp(al, "text")) fc->type = FC_TEXT;
	else if (!strcasecmp(al, "password")) fc->type = FC_PASSWORD;
	else if (!strcasecmp(al, "checkbox")) fc->type = FC_CHECKBOX;
	else if (!strcasecmp(al, "radio")) fc->type = FC_RADIO;
	else if (!strcasecmp(al, "submit")) fc->type = FC_SUBMIT;
	else if (!strcasecmp(al, "reset")) fc->type = FC_RESET;
	else if (!strcasecmp(al, "file")) fc->type = FC_FILE;
	else if (!strcasecmp(al, "hidden")) fc->type = FC_HIDDEN;
	else if (!strcasecmp(al, "image")) fc->type = FC_IMAGE;
	else if (!strcasecmp(al, "button")) {
		mem_free(al);
		put_chrs(" [&nbsp;", 8, put_chars_f, ff);

		al = get_attr_val(a, "value");
		if (al) {
			put_chrs(al, strlen(al), put_chars_f, ff);
			mem_free(al);
		} else put_chrs("BUTTON", 6, put_chars_f, ff);

		put_chrs("&nbsp;] ", 8, put_chars_f, ff);
		mem_free(fc);
		return;
	} else fc->type = FC_TEXT;
	mem_free(al);

xxx:
	fc->form_num = last_form_tag - startf;
	fc->ctrl_num = a - last_form_tag;
	fc->position = a - startf;
	fc->method = form.method;
	fc->action = form.action ? stracpy(form.action) : NULL;
	fc->target = form.target ? stracpy(form.target) : NULL;
	fc->name = get_attr_val(a, "name");

	if (fc->type != FC_FILE) fc->default_value = get_attr_val(a, "value");
	if (fc->type == FC_CHECKBOX && !fc->default_value) fc->default_value = stracpy("on");
	fc->size = get_num(a, "size");
	if (fc->size == -1) fc->size = HTML_DEFAULT_INPUT_SIZE;
	fc->size++;
	if (fc->size > d_opt->xw) fc->size = d_opt->xw;
	fc->maxlength = get_num(a, "maxlength");
	if (fc->maxlength == -1) fc->maxlength = MAXINT;
	if (fc->type == FC_CHECKBOX || fc->type == FC_RADIO) fc->default_state = has_attr(a, "checked");
	fc->ro = has_attr(a, "disabled") ? 2 : has_attr(a, "readonly") ? 1 : 0;
	if (fc->type == FC_IMAGE) fc->alt = get_attr_val(a, "alt");
	if (fc->type == FC_SUBMIT && !fc->default_value) fc->default_value = stracpy("Submit");
	if (fc->type == FC_RESET && !fc->default_value) fc->default_value = stracpy("Reset");
	if (!fc->default_value) fc->default_value = stracpy("");
	if (fc->type == FC_HIDDEN) goto hid;

	put_chrs(" ", 1, put_chars_f, ff);
	html_stack_dup();
	format.form = fc;
	if (format.title) mem_free(format.title);
	format.title = get_attr_val(a, "title");
	switch (fc->type) {
		case FC_TEXT:
		case FC_PASSWORD:
		case FC_FILE:
			format.attr |= AT_BOLD;
			for (i = 0; i < fc->size; i++) put_chrs("_", 1, put_chars_f, ff);
			break;
		case FC_CHECKBOX:
			format.attr |= AT_BOLD;
			put_chrs("[&nbsp;]", 8, put_chars_f, ff);
			break;
		case FC_RADIO:
			format.attr |= AT_BOLD;
			put_chrs("(&nbsp;)", 8, put_chars_f, ff);
			break;
		case FC_IMAGE:
			if (format.image) mem_free(format.image), format.image = NULL;
			if ((al = get_url_val(a, "src"))
			    || (al = get_url_val(a, "dynsrc"))) {
				format.image = join_urls(format.href_base, al);
				mem_free(al);
			}
			format.attr |= AT_BOLD;
			put_chrs("[&nbsp;", 7, put_chars_f, ff);
			if (fc->alt) put_chrs(fc->alt, strlen(fc->alt), put_chars_f, ff);
			else if (fc->name) put_chrs(fc->name, strlen(fc->name), put_chars_f, ff);
			else put_chrs("Submit", 6, put_chars_f, ff);
			put_chrs("&nbsp;]", 7, put_chars_f, ff);
			break;
		case FC_SUBMIT:
		case FC_RESET:
			format.attr |= AT_BOLD;
			put_chrs("[&nbsp;", 7, put_chars_f, ff);
			if (fc->default_value) put_chrs(fc->default_value, strlen(fc->default_value), put_chars_f, ff);
			put_chrs("&nbsp;]", 7, put_chars_f, ff);
			break;
		default:
			internal("bad control type");
	}
	kill_html_stack_item(&html_top);
	put_chrs(" ", 1, put_chars_f, ff);

hid:
	special_f(ff, SP_CONTROL, fc);
}

static void
html_select(unsigned char *a)
{
	/* Note I haven't seen this code in use, do_html_select() seems to take
	 * care of bussiness. --FF */

	unsigned char *al = get_attr_val(a, "name");

	if (!al) return;
	html_focusable(a);
	html_top.dontkill = 1;
	format.select = al;
	format.select_disabled = 2 * has_attr(a, "disabled");
}

static void
html_option(unsigned char *a)
{
	struct form_control *fc;
	unsigned char *val;

	find_form_for_input(a);
	if (!format.select) return;

	fc = mem_calloc(1, sizeof(struct form_control));
	if (!fc) return;

	val = get_attr_val(a, "value");
	if (!val) {
		struct string str;
		unsigned char *p, *r;
		unsigned char *name;
		int namelen;

		for (p = a - 1; *p != '<'; p--);

		if (!init_string(&str)) goto x;
		if (parse_element(p, eoff, NULL, NULL, NULL, &p)) {
			internal("parse element failed");
			val = str.source;
			goto x;
		}

rrrr:
		while (p < eoff && WHITECHAR(*p)) p++;
		while (p < eoff && !WHITECHAR(*p) && *p != '<') {

pppp:
			add_char_to_string(&str, *p), p++;
		}

		r = p;
		val = str.source; /* Has to be before the possible 'goto x' */

		while (r < eoff && WHITECHAR(*r)) r++;
		if (r >= eoff) goto x;
		if (r - 2 <= eoff && (r[1] == '!' || r[1] == '?')) {
			p = skip_comment(r, eoff);
			goto rrrr;
		}
		if (parse_element(r, eoff, &name, &namelen, NULL, &p)) goto pppp;
		if (!((namelen == 6 && !strncasecmp(name, "OPTION", 6)) ||
		    (namelen == 7 && !strncasecmp(name, "/OPTION", 7)) ||
		    (namelen == 6 && !strncasecmp(name, "SELECT", 6)) ||
		    (namelen == 7 && !strncasecmp(name, "/SELECT", 7)) ||
		    (namelen == 8 && !strncasecmp(name, "OPTGROUP", 8)) ||
		    (namelen == 9 && !strncasecmp(name, "/OPTGROUP", 9)))) goto rrrr;
	}

x:
	fc->form_num = last_form_tag - startf;
	fc->ctrl_num = a - last_form_tag;
	fc->position = a - startf;
	fc->method = form.method;
	fc->action = form.action ? stracpy(form.action) : NULL;
	fc->type = FC_CHECKBOX;
	fc->name = format.select ? stracpy(format.select) : NULL;
	fc->default_value = val;
	fc->default_state = has_attr(a, "selected");
	fc->ro = format.select_disabled;
	if (has_attr(a, "disabled")) fc->ro = 2;
	put_chrs(" ", 1, put_chars_f, ff);
	html_stack_dup();
	format.form = fc;
	format.attr |= AT_BOLD;
	put_chrs("[ ]", 3, put_chars_f, ff);
	kill_html_stack_item(&html_top);
	put_chrs(" ", 1, put_chars_f, ff);
	special_f(ff, SP_CONTROL, fc);
}

static void
clr_spaces(unsigned char *name)
{
	unsigned char *nm, *second;

	for (nm = name; *nm; nm++)
		if (WHITECHAR(*nm) || *nm == 1) *nm = ' ';
	for (second = nm = name; *nm; nm++) {
		if (nm[0] == ' ' && (second == name || nm[1] == ' ' || !nm[1]))
			continue;
		*second++ = *nm;
	}
	/* FIXME: Shouldn't be *second = 0; enough? --pasky */
	memset(second, 0, nm - second);
}


/* TODO: Move following stuff to special file. */

static int menu_stack_size;
static struct menu_item **menu_stack;

static void
new_menu_item(unsigned char *name, int data, int fullname)
	/* name == NULL - up;	data == -1 - down */
{
	struct menu_item *top, *item, *nmenu = NULL; /* no uninitialized warnings */

	if (name) {
		clr_spaces(name);
		if (!name[0]) mem_free(name), name = stracpy(" ");
		if (name[0] == 1) name[0] = ' ';
	}

	if (name && data == -1) {
		nmenu = mem_calloc(1, sizeof(struct menu_item));
		if (!nmenu) {
			mem_free(name);
			return;
		}
		/*nmenu->text = "";*/
	}

	if (menu_stack_size && name) {
		top = item = menu_stack[menu_stack_size - 1];
		while (item->text) item++;

		top = mem_realloc(top, (char *)(item + 2) - (char *)top);
		if (!top) {
			if (data == -1) mem_free(nmenu);
			mem_free(name);
			return;
		}
		item = item - menu_stack[menu_stack_size - 1] + top;
		menu_stack[menu_stack_size - 1] = top;
		if (menu_stack_size >= 2) {
			struct menu_item *below = menu_stack[menu_stack_size - 2];

			while (below->text) below++;
			below[-1].data = top;
		}

		if (data == -1) {
			SET_MENU_ITEM(item, name, M_SUBMENU, do_select_submenu,
				      nmenu, FREE_NOTHING | (fullname << 8),
				      1, 1, 0, 0);
		} else {
			SET_MENU_ITEM(item, name, "", selected_item,
				      data, FREE_NOTHING | (fullname << 8),
				      0, 1, 0, 0);
		}

		item++;
		memset(item, 0, sizeof(struct menu_item));
		/*item->text = "";*/
	} else if (name) mem_free(name);

	if (name && data == -1) {
		struct menu_item **ms = mem_realloc(menu_stack, (menu_stack_size + 1) * sizeof(struct menu_item *));

		if (!ms) return;
		menu_stack = ms;
		menu_stack[menu_stack_size++] = nmenu;
	}

	if (!name) menu_stack_size--;
}

static inline void
init_menu(void)
{
	menu_stack_size = 0;
	menu_stack = NULL;
	new_menu_item(stracpy(""), -1, 0);
}

void
free_menu(struct menu_item *m) /* Grrr. Recursion */
{
	struct menu_item *mm;

	if (!m) return; /* XXX: Who knows... need to be verified */

	for (mm = m; mm->text; mm++) {
		if (mm->text) mem_free(mm->text);
		if (mm->func == (menu_func) do_select_submenu) free_menu(mm->data);
	}
	mem_free(m);
}

static inline struct menu_item *
detach_menu(void)
{
	struct menu_item *i = NULL;

	if (menu_stack) {
		if (menu_stack_size) i = menu_stack[0];
		mem_free(menu_stack);
	}

	return i;
}

static inline void
destroy_menu(void)
{
	if (menu_stack) free_menu(menu_stack[0]);
	detach_menu();
}

static void
menu_labels(struct menu_item *m, unsigned char *base, unsigned char **lbls)
{
	unsigned char *bs;

	for (; m->text; m++) {
		if (m->func == (menu_func) do_select_submenu) {
			bs = stracpy(base);
			if (bs) {
				add_to_strn(&bs, m->text);
				add_to_strn(&bs, " ");
				menu_labels(m->data, bs, lbls);
				mem_free(bs);
			}
		} else {
			bs = stracpy((m->item_free & (1<<8)) ? (unsigned char *)"" : base);
			if (bs) add_to_strn(&bs, m->text);
			lbls[(int)m->data] = bs;
		}
	}
}

static int
menu_contains(struct menu_item *m, int f)
{
	if (m->func != (menu_func) do_select_submenu)
		return (int)m->data == f;
	for (m = m->data; m->text; m++)
		if (menu_contains(m, f))
			return 1;
	return 0;
}

void
do_select_submenu(struct terminal *term, struct menu_item *menu,
		  struct session *ses)
{
	struct menu_item *m;
	int def = get_current_state(ses);
	int sel = 0;

	if (def < 0) def = 0;
	for (m = menu; m->text; m++, sel++)
		if (menu_contains(m, def))
			goto found;
	sel = 0;

found:
	do_menu_selected(term, menu, ses, sel, 0);
}

static int
do_html_select(unsigned char *attr, unsigned char *html,
	       unsigned char *eof, unsigned char **end, void *f)
{
	struct conv_table *ct = special_f(f, SP_TABLE, NULL);
	struct form_control *fc;
	struct string lbl = NULL_STRING;
	unsigned char **val, **lbls;
	unsigned char *t_name, *t_attr, *en;
	int t_namelen;
	int nnmi = 0;
	int order, preselect, group;
	int i, mw;

	if (has_attr(attr, "multiple")) return 1;
	find_form_for_input(attr);
	html_focusable(attr);
	val = NULL;
	order = 0, group = 0, preselect = -1;
	init_menu();

se:
        en = html;

see:
        html = en;
	while (html < eof && *html != '<') html++;

	if (html >= eof) {
		int j;

abort:
		*end = html;
		if (lbl.source) done_string(&lbl);
		if (val) {
			for (j = 0; j < order; j++)
				if (val[j])
					mem_free(val[j]);
			mem_free(val);
		}
		destroy_menu();
		*end = en;
		return 0;
	}

	if (lbl.source) {
		unsigned char *q, *s = en;
		int l = html - en;

		while (l && WHITECHAR(s[0])) s++, l--;
		while (l && WHITECHAR(s[l-1])) l--;
		q = convert_string(ct, s, l, CSM_DEFAULT);
		if (q) add_to_string(&lbl, q), mem_free(q);
	}

	if (html + 2 <= eof && (html[1] == '!' || html[1] == '?')) {
		html = skip_comment(html, eof);
		goto se;
	}

	if (parse_element(html, eof, &t_name, &t_namelen, &t_attr, &en)) {
		html++;
		goto se;
	}

#define add_select_item(string, value, order, dont_add) 		\
	do {								\
		if (!(string).source) break;				\
									\
		if (!value[(order) - 1])				\
			value[(order) - 1] = memacpy((string).source,	\
						     (string).length);	\
		if (dont_add) {						\
			done_string(&(string));				\
		} else {						\
			new_menu_item((string).source, (order) - 1, 1);	\
			(string).source = NULL;				\
			(string).length = 0;				\
		}							\
	} while (0)

	if (t_namelen == 7 && !strncasecmp(t_name, "/SELECT", 7)) {
		add_select_item(lbl, val, order, nnmi);
		goto end_parse;
	}

	if (t_namelen == 7 && !strncasecmp(t_name, "/OPTION", 7)) {
		add_select_item(lbl, val, order, nnmi);
		goto see;
	}

	if (t_namelen == 6 && !strncasecmp(t_name, "OPTION", 6)) {
		unsigned char *v, *vx;

		add_select_item(lbl, val, order, nnmi);

		if (has_attr(t_attr, "disabled")) goto see;
		if (preselect == -1 && has_attr(t_attr, "selected")) preselect = order;
		v = get_attr_val(t_attr, "value");

		if (!mem_align_alloc(&val, order, order + 1, sizeof(unsigned char *), 0xFF))
			goto abort;

		val[order++] = v;
		vx = get_attr_val(t_attr, "label");
		if (vx) new_menu_item(vx, order - 1, 0);
		if (!v || !vx) {
			init_string(&lbl);
			nnmi = !!vx;
		}
		goto see;
	}

	if ((t_namelen == 8 && !strncasecmp(t_name, "OPTGROUP", 8))
	    || (t_namelen == 9 && !strncasecmp(t_name, "/OPTGROUP", 9))) {
		add_select_item(lbl, val, order, nnmi);

		if (group) new_menu_item(NULL, -1, 0), group = 0;
	}

#undef add_select_item

	if (t_namelen == 8 && !strncasecmp(t_name, "OPTGROUP", 8)) {
		unsigned char *la = get_attr_val(t_attr, "label");

		if (!la) {
			la = stracpy("");
			if (!la) goto see;
		}
		new_menu_item(la, -1, 0);
		group = 1;
	}
	goto see;


end_parse:
	*end = en;
	if (!order) goto abort;

	fc = mem_calloc(1, sizeof(struct form_control));
	if (!fc) goto abort;

	lbls = mem_calloc(order, sizeof(unsigned char *));
	if (!lbls) {
		mem_free(fc);
		goto abort;
	}

	fc->form_num = last_form_tag - startf;
	fc->ctrl_num = attr - last_form_tag;
	fc->position = attr - startf;
	fc->method = form.method;
	fc->action = form.action ? stracpy(form.action) : NULL;
	fc->name = get_attr_val(attr, "name");
	fc->type = FC_SELECT;
	fc->default_state = preselect < 0 ? 0 : preselect;
	fc->default_value = order ? stracpy(val[fc->default_state]) : stracpy("");
	fc->ro = has_attr(attr, "disabled") ? 2 : has_attr(attr, "readonly") ? 1 : 0;
	fc->nvalues = order;
	fc->values = val;
	fc->menu = detach_menu();
	fc->labels = lbls;
	menu_labels(fc->menu, "", lbls);
	put_chrs("[", 1, put_chars_f, f);
	html_stack_dup();
	format.form = fc;
	format.attr |= AT_BOLD;
	mw = 0;

	for (i = 0; i < order; i++) {
		if (lbls[i]) {
			int len = strlen(lbls[i]);

			if (len > mw) mw = len;
		}
	}

	for (i = 0; i < mw; i++)
		put_chrs("_", 1, put_chars_f, f);

	kill_html_stack_item(&html_top);
	put_chrs("]", 1, put_chars_f, f);
	special_f(ff, SP_CONTROL, fc);

	return 0;
}



static void
html_textarea(unsigned char *a)
{
	internal("This should be never called");
}

static void
do_html_textarea(unsigned char *attr, unsigned char *html, unsigned char *eof,
		 unsigned char **end, void *f)
{
	struct form_control *fc;
	unsigned char *p, *t_name, *wrap_attr;
	int t_namelen;
	int cols, rows;
	int i;

	find_form_for_input(attr);
	html_focusable(attr);
	while (html < eof && (*html == '\n' || *html == '\r')) html++;
	p = html;
	while (p < eof && *p != '<') {
		pp:
		p++;
	}
	if (p >= eof) {
		*end = eof;
		return;
	}
	if (parse_element(p, eof, &t_name, &t_namelen, NULL, end)) goto pp;
	if (t_namelen != 9 || strncasecmp(t_name, "/TEXTAREA", 9)) goto pp;

	fc = mem_calloc(1, sizeof(struct form_control));
	if (!fc) return;

	fc->form_num = last_form_tag - startf;
	fc->ctrl_num = attr - last_form_tag;
	fc->position = attr - startf;
	fc->method = form.method;
	fc->action = form.action ? stracpy(form.action) : NULL;
	fc->name = get_attr_val(attr, "name");
	fc->type = FC_TEXTAREA;;
	fc->ro = has_attr(attr, "disabled") ? 2 : has_attr(attr, "readonly") ? 1 : 0;
	fc->default_value = memacpy(html, p - html);
	for (p = fc->default_value; p && p[0]; p++) {
		/* FIXME: We don't cope well with entities here. Bugzilla uses
		 * &#13; inside of textarea and we fail miserably upon that
		 * one.  --pasky */
		if (p[0] == '\r') {
			if (p[1] == '\n' || (p > fc->default_value && p[-1] == '\n')) {
				memcpy(p, p + 1, strlen(p)), p--;
			} else {
				p[0] = '\n';
			}
		}
	}

	cols = get_num(attr, "cols");
	if (cols <= 0) cols = HTML_DEFAULT_INPUT_SIZE;
	cols++; /* Add 1 column, other browsers may have different
		   behavior here (mozilla adds 2) --Zas */
	if (cols > d_opt->xw) cols = d_opt->xw;
	fc->cols = cols;

	rows = get_num(attr, "rows");
	if (rows <= 0) rows = 1;
	if (rows > d_opt->yw) rows = d_opt->yw;
	fc->rows = rows;

	wrap_attr = get_attr_val(attr, "wrap");
	if (wrap_attr) {
		if (!strcasecmp(wrap_attr, "hard")
		    || !strcasecmp(wrap_attr, "physical")) {
			fc->wrap = 2;
		} else if (!strcasecmp(wrap_attr, "soft")
			   || !strcasecmp(wrap_attr, "virtual")) {
			fc->wrap = 1;
		} else if (!strcasecmp(wrap_attr, "none")
			   || !strcasecmp(wrap_attr, "off")) {
			fc->wrap = 0;
		}
		mem_free(wrap_attr);
	} else {
		fc->wrap = 1;
	}

	fc->maxlength = get_num(attr, "maxlength");
	if (fc->maxlength == -1) fc->maxlength = MAXINT;

	if (rows > 1) ln_break(1, line_break_f, f);
	else put_chrs(" ", 1, put_chars_f, f);

	html_stack_dup();
	format.form = fc;
	format.attr |= AT_BOLD;

	for (i = 0; i < rows; i++) {
		int j;

		for (j = 0; j < cols; j++)
			put_chrs("_", 1, put_chars_f, f);
		if (i < rows - 1)
			ln_break(1, line_break_f, f);
	}

	kill_html_stack_item(&html_top);
	if (rows > 1) ln_break(1, line_break_f, f);
	else put_chrs(" ", 1, put_chars_f, f);
	special_f(f, SP_CONTROL, fc);
}

static void
html_iframe(unsigned char *a)
{
	unsigned char *name, *url;

	url = get_url_val(a, "src");
	if (!url) return;

	name = get_attr_val(a, "name");
	if (!name) {
		name = stracpy("");
		if (!name) {
			mem_free(url);
			return;
		}
	}

	html_focusable(a);

	if (*name) {
		put_link_line("IFrame: ", name, url, d_opt->framename);
	} else {
		put_link_line("", "IFrame", url, d_opt->framename);
	}

	mem_free(name);
	mem_free(url);
}

static void
html_noframes(unsigned char *a)
{
	if (d_opt->frames) html_skip(a);
}

static void
html_frame(unsigned char *a)
{
	unsigned char *name, *src, *url;

	src = get_url_val(a, "src");
	if (!src) {
		url = stracpy("");
	} else {
		url = join_urls(format.href_base, src);
		mem_free(src);
	}
	if (!url) return;

	name = get_attr_val(a, "name");
	if (!name) {
		name = stracpy(url);
	} else if (!name[0]) {
		/* When name doesn't have a value */
		mem_free(name);
		name = stracpy(url);
	}
	if (!name) return;

	if (!d_opt->frames || !html_top.frameset) {
		html_focusable(a);
		put_link_line("Frame: ", name, url, "");

	} else {
		if (special_f(ff, SP_USED, NULL)) {
			struct frame_param fp;

			fp.name = name;
			fp.url = url;
			fp.parent = html_top.frameset;

			special_f(ff, SP_FRAME, &fp);
		}
	}

	mem_free(name);
	mem_free(url);
}

static void
parse_frame_widths(unsigned char *a, int ww, int www, int **op, int *olp)
{
	unsigned char *aa;
	unsigned long n;
	int q, qq, d, nn;
	int *oo;
	int *o = NULL;
	int ol = 0;
	register int i;

new_ch:
	while (WHITECHAR(*a)) a++;
	errno = 0;
	n = strtoul(a, (char **)&a, 10);
	if (errno) {
		*olp = 0;
		return;
	}

	q = n;
	if (*a == '%') q = q * ww / 100;
	else if (*a != '*') q = (q + (www - 1) / 2) / www;
	else if (!q) q = -1;
	else q = -q;

	oo = mem_realloc(o, (ol + 1) * sizeof(int));
	if (oo) (o = oo)[ol++] = q;
	else {
		*olp = 0;
		return;
	}
	aa = strchr(a, ',');
	if (aa) {
		a = aa + 1;
		goto new_ch;
	}
	*op = o;
	*olp = ol;
	q = 2 * ol - 1;
	for (i = 0; i < ol; i++) if (o[i] > 0) q += o[i] - 1;

	if (q >= ww) {

distribute:
		for (i = 0; i < ol; i++) if (o[i] < 1) o[i] = 1;
		q -= ww;
		d = 0;
		for (i = 0; i < ol; i++) d += o[i];
		qq = q;
		for (i = 0; i < ol; i++) {
			q -= o[i] - o[i] * (d - qq) / d;
			/* SIGH! gcc 2.7.2.* has an optimizer bug! */
			do_not_optimize_here_gcc_2_7(&d);
			o[i] = o[i] * (d - qq) / d;
		}
		while (q) {
			nn = 0;
			for (i = 0; i < ol; i++) {
				if (q < 0) o[i]++, q++, nn = 1;
				if (q > 0 && o[i] > 1) o[i]--, q--, nn = 1;
				if (!q) break;
			}
			if (!nn) break;
		}
	} else {
		int neg = 0;

		for (i = 0; i < ol; i++) if (o[i] < 0) neg = 1;
		if (!neg) goto distribute;

		oo = mem_alloc(ol * sizeof(int));
		if (!oo) {
			*olp = 0;
			return;
		}
		memcpy(oo, o, ol * sizeof(int));
		for (i = 0; i < ol; i++) if (o[i] < 1) o[i] = 1;
		q = ww - q;
		d = 0;
		for (i = 0; i < ol; i++) if (oo[i] < 0) d += -oo[i];
		qq = q;
		for (i = 0; i < ol; i++) if (oo[i] < 0) {
			o[i] += (-oo[i] * qq / d);
			q -= (-oo[i] * qq / d);
		}
		assertm(q >= 0, "parse_frame_widths: q < 0");
		if_assert_failed q = 0;
		for (i = 0; i < ol; i++) if (oo[i] < 0) {
			if (q) o[i]++, q--;
		}
		assertm(q <= 0, "parse_frame_widths: q > 0");
		if_assert_failed q = 0;
		mem_free(oo);
	}

	for (i = 0; i < ol; i++) if (!o[i]) {
		register int j;
		int m = 0;
		int mj = 0;

		for (j = 0; j < ol; j++) if (o[j] > m) m = o[j], mj = j;
		if (m) o[i] = 1, o[mj]--;
	}
}

static void
html_frameset(unsigned char *a)
{
	struct frameset_param fp;
	unsigned char *c, *d;
	int x, y;

	if (!d_opt->frames || !special_f(ff, SP_USED, NULL)) return;

	c = get_attr_val(a, "cols");
	if (!c) {
		c = stracpy("100%");
		if (!c) return;
	}

	d = get_attr_val(a, "rows");
	if (!d) {
		d = stracpy("100%");
	       	if (!d) {
			mem_free(c);
			return;
		}
	}

	if (!html_top.frameset) {
		x = d_opt->xw;
		y = d_opt->yw;
	} else {
		struct frameset_desc *f = html_top.frameset;

		if (f->yp >= f->y) goto free_cd;
		x = f->f[f->xp + f->yp * f->x].xw;
		y = f->f[f->xp + f->yp * f->x].yw;
	}

	parse_frame_widths(c, x, HTML_FRAME_CHAR_WIDTH, &fp.xw, &fp.x);
	parse_frame_widths(d, y, HTML_FRAME_CHAR_HEIGHT, &fp.yw, &fp.y);
	fp.parent = html_top.frameset;
	if (fp.x && fp.y) html_top.frameset = special_f(ff, SP_FRAMESET, &fp);
	mem_free(fp.xw);
	mem_free(fp.yw);

free_cd:
	mem_free(c);
	mem_free(d);
}

/* Link types

Alternate
	Designates substitute versions for the document in which the link
	occurs. When used together with the lang attribute, it implies a
	translated version of the document. When used together with the
	media attribute, it implies a version designed for a different
	medium (or media).

Stylesheet
	Refers to an external style sheet. See the section on external style
	sheets for details. This is used together with the link type
	"Alternate" for user-selectable alternate style sheets.

Start
	Refers to the first document in a collection of documents. This link
	type tells search engines which document is considered by the author
	to be the starting point of the collection.

Next
	Refers to the next document in a linear sequence of documents. User
	agents may choose to preload the "next" document, to reduce the
	perceived load time.

Prev
	Refers to the previous document in an ordered series of documents.
	Some user agents also support the synonym "Previous".

Contents
	Refers to a document serving as a table of contents.
	Some user agents also support the synonym ToC (from "Table of Contents").

Index
	Refers to a document providing an index for the current document.

Glossary
	Refers to a document providing a glossary of terms that pertain to the
	current document.

Copyright
	Refers to a copyright statement for the current document.

Chapter
        Refers to a document serving as a chapter in a collection of documents.

Section
	Refers to a document serving as a section in a collection of documents.

Subsection
	Refers to a document serving as a subsection in a collection of
	documents.

Appendix
	Refers to a document serving as an appendix in a collection of
	documents.

Help
	Refers to a document offering help (more information, links to other
	sources information, etc.)

Bookmark
	Refers to a bookmark. A bookmark is a link to a key entry point
	within an extended document. The title attribute may be used, for
	example, to label the bookmark. Note that several bookmarks may be
	defined in each document.

Some were added like top, ... --Zas
*/
enum hlink_type {
	LT_UNKNOWN = 0,
	LT_START,
	LT_PARENT,
	LT_NEXT,
	LT_PREV,
	LT_CONTENTS,
	LT_INDEX,
	LT_GLOSSARY,
	LT_CHAPTER,
	LT_SECTION,
	LT_SUBSECTION,
	LT_APPENDIX,
	LT_HELP,
	LT_SEARCH,
	LT_BOOKMARK,
	LT_COPYRIGHT,
	LT_AUTHOR,
	LT_ICON,
	LT_ALTERNATE,
	LT_ALTERNATE_LANG,
	LT_ALTERNATE_MEDIA,
	LT_ALTERNATE_STYLESHEET,
	LT_STYLESHEET,
};

enum hlink_direction {
	LD_UNKNOWN = 0,
	LD_REV,
	LD_REL,
};

struct hlink {
	enum hlink_type type;
	enum hlink_direction direction;
	unsigned char *content_type;
	unsigned char *media;
	unsigned char *href;
	unsigned char *hreflang;
	unsigned char *title;
	unsigned char *lang;
	unsigned char *name;
/* Not used implemented.
	unsigned char *charset;
	unsigned char *target;
	unsigned char *id;
	unsigned char *class;
	unsigned char *dir;
*/
};

struct lt_default_name {
	enum hlink_type type;
	unsigned char *str;
};

/* TODO: i18n */
static struct lt_default_name lt_names[] = {
	{ LT_START, "start" },
	{ LT_PARENT, "parent" },
	{ LT_NEXT, "next" },
	{ LT_PREV, "previous" },
	{ LT_CONTENTS, "contents" },
	{ LT_INDEX, "index" },
	{ LT_GLOSSARY, "glossary" },
	{ LT_CHAPTER, "chapter" },
	{ LT_SECTION, "section" },
	{ LT_SUBSECTION, "subsection" },
	{ LT_APPENDIX, "appendix" },
	{ LT_HELP, "help" },
	{ LT_SEARCH, "search" },
	{ LT_BOOKMARK, "bookmark" },
	{ LT_ALTERNATE_LANG, "alt. language" },
	{ LT_ALTERNATE_MEDIA, "alt. media" },
	{ LT_ALTERNATE_STYLESHEET, "alt. stylesheet" },
	{ LT_STYLESHEET, "stylesheet" },
	{ LT_ALTERNATE, "alternate" },
	{ LT_COPYRIGHT, "copyright" },
	{ LT_AUTHOR, "author" },
	{ LT_ICON, "icon" },
	{ LT_UNKNOWN, NULL }
};

/* Search for default name for this link according to its type. */
static unsigned char *
get_lt_default_name(struct hlink *link)
{
	struct lt_default_name *entry = lt_names;

	assert(link);

	while (entry && entry->str) {
		if (entry->type == link->type) return entry->str;
		entry++;
	}

	return "unknown";
}

static void
html_link_clear(struct hlink *link)
{
	assert(link);

	if (link->content_type) mem_free(link->content_type);
	if (link->media) mem_free(link->media);
	if (link->href) mem_free(link->href);
	if (link->hreflang) mem_free(link->hreflang);
	if (link->title) mem_free(link->title);
	if (link->lang) mem_free(link->lang);
	if (link->name) mem_free(link->name);

	memset(link, 0, sizeof(struct hlink));
}

/* Parse a link and return results in @link.
 * It tries to identify known types. */
static int
html_link_parse(unsigned char *a, struct hlink *link)
{
	assert(a && link);
	memset(link, 0, sizeof(struct hlink));

	link->href = get_url_val(a, "href");
	if (!link->href) return 0;

	link->lang = get_attr_val(a, "lang");
	link->hreflang = get_attr_val(a, "hreflang");
	link->title = get_attr_val(a, "title");
	link->content_type = get_attr_val(a, "type");
	link->media = get_attr_val(a, "media");

	link->name = get_attr_val(a, "rel");
	if (link->name) link->direction = LD_REL;
	else {
		link->name = get_attr_val(a, "rev");
		if (link->name) link->direction = LD_REV;
	}

	if (!link->name) return 1;

	/* TODO: fastfind */
	if (!strcasecmp(link->name, "start") ||
	    !strcasecmp(link->name, "top") ||
	    !strcasecmp(link->name, "home"))
		link->type = LT_START;
	else if (!strcasecmp(link->name, "parent") ||
		 !strcasecmp(link->name, "up"))
		link->type = LT_PARENT;
	else if (!strcasecmp(link->name, "next"))
		link->type = LT_NEXT;
	else if (!strcasecmp(link->name, "prev") ||
		 !strcasecmp(link->name, "previous"))
		link->type = LT_PREV;
	else if (!strcasecmp(link->name, "contents") ||
		 !strcasecmp(link->name, "toc"))
		link->type = LT_CONTENTS;
	else if (!strcasecmp(link->name, "index"))
		link->type = LT_INDEX;
	else if (!strcasecmp(link->name, "glossary"))
		link->type = LT_GLOSSARY;
	else if (!strcasecmp(link->name, "chapter"))
		link->type = LT_CHAPTER;
	else if (!strcasecmp(link->name, "section"))
		link->type = LT_SECTION;
	else if (!strcasecmp(link->name, "subsection") ||
		 !strcasecmp(link->name, "child") ||
		 !strcasecmp(link->name, "sibling"))
		link->type = LT_SUBSECTION;
	else if (!strcasecmp(link->name, "appendix"))
		link->type = LT_APPENDIX;
	else if (!strcasecmp(link->name, "help"))
		link->type = LT_HELP;
	else if (!strcasecmp(link->name, "search"))
		link->type = LT_SEARCH;
	else if (!strcasecmp(link->name, "bookmark"))
		link->type = LT_BOOKMARK;
	else if (!strcasecmp(link->name, "copyright"))
		link->type = LT_COPYRIGHT;
	else if (!strcasecmp(link->name, "author") ||
		 !strcasecmp(link->name, "made") ||
		 !strcasecmp(link->name, "owner"))
		link->type = LT_AUTHOR;
	else if (strcasestr(link->name, "icon") ||
		 (link->content_type && strcasestr(link->content_type, "icon")))
		link->type = LT_ICON;
	else if (strcasestr(link->name, "alternate")) {
		link->type = LT_ALTERNATE;
		if (link->lang)
			link->type = LT_ALTERNATE_LANG;
		else if (strcasestr(link->name, "stylesheet") ||
			 (link->content_type && strcasestr(link->content_type, "css")))
			link->type = LT_ALTERNATE_STYLESHEET;
		else if (link->media)
			link->type = LT_ALTERNATE_MEDIA;
	} else if (!strcasecmp(link->name, "stylesheet") ||
		   (link->content_type && strcasestr(link->content_type, "css")))
		link->type = LT_STYLESHEET;

	return 1;
}

static void
html_link(unsigned char *a)
{
	int link_display = d_opt->meta_link_display;
	unsigned char *name = NULL;
	struct hlink link;
	static unsigned char link_rel_string[] = "Link: ";
	static unsigned char link_rev_string[] = "Reverse link: ";

	if (!link_display) return;
	if (!html_link_parse(a, &link)) return;

	/* Ignore few annoying links.. */
	if (link_display < 5 &&
	    (link.type == LT_ICON ||
	     link.type == LT_AUTHOR ||
	     link.type == LT_STYLESHEET ||
	     link.type == LT_ALTERNATE_STYLESHEET)) goto free_and_return;


	if (!link.name || link.type != LT_UNKNOWN)
		/* Give preference to our default names for known types. */
		name = get_lt_default_name(&link);
	else
		name = link.name;

	if (name && link.href) {
		struct string text;
		int name_neq_title = 0;
		int first = 1;

		if (!init_string(&text)) goto free_and_return;

		html_focusable(a);

		if (link.title) {
			add_to_string(&text, link.title);
			name_neq_title = strcmp(link.title, name);
		} else
			add_to_string(&text, name);

		if (link_display == 1) goto only_title;

		if (name_neq_title) {
			if (!first) add_to_string(&text, ", ");
			else add_to_string(&text, " (");
			add_to_string(&text, name);
			first = 0;
		}

		if (link_display >= 3 && link.hreflang) {
			if (!first) add_to_string(&text, ", ");
			else add_to_string(&text, " (");
			add_to_string(&text, link.hreflang);
			first = 0;
		}

		if (link_display >= 4 && link.content_type) {
			if (!first) add_to_string(&text, ", ");
			else add_to_string(&text, " (");
			add_to_string(&text, link.content_type);
			first = 0;
		}

		if (link.lang && link.type == LT_ALTERNATE_LANG &&
		    (link_display < 3 || (link.hreflang &&
					  strcasecmp(link.hreflang, link.lang)))) {
			if (!first) add_to_string(&text, ", ");
			else add_to_string(&text, " (");
			add_to_string(&text, link.lang);
			first = 0;
		}

		if (link.media) {
			if (!first) add_to_string(&text, ", ");
			else add_to_string(&text, " (");
			add_to_string(&text, link.media);
			first = 0;
		}

		if (!first) add_char_to_string(&text, ')');

only_title:
		if (text.length)
			put_link_line((link.direction == LD_REL) ? link_rel_string :  link_rev_string,
				      text.source, link.href, format.target_base);
		else
			put_link_line((link.direction == LD_REL) ? link_rel_string :  link_rev_string,
				      name, link.href, format.target_base);

		if (text.source) done_string(&text);
	}

free_and_return:
	html_link_clear(&link);
}

struct element_info {
	unsigned char *name;
	void (*func)(unsigned char *);
	int linebreak;
	int nopair;
};

#define NUMBER_OF_TAGS 64

static struct element_info elements[] = {
	{"A",		html_a,		0, 2},
	{"ABBR",	html_italic,	0, 0},
	{"ADDRESS",	html_address,	2, 0},
	{"B",		html_bold,	0, 0},
	{"BASE",	html_base,	0, 1},
	{"BASEFONT",	html_font,	0, 1},
	{"BLOCKQUOTE",	html_blockquote,2, 0},
	{"BODY",	html_body,	0, 0},
	{"BR",		html_br,	1, 1},
	{"BUTTON",	html_button,	0, 0},
	{"CAPTION",	html_center,	1, 0},
	{"CENTER",	html_center,	1, 0},
	{"CODE",	html_fixed,	0, 0},
	{"DD",		html_dd,	1, 1},
	{"DFN",		html_bold,	0, 0},
	{"DIR",		html_ul,	2, 0},
	{"DIV",		html_linebrk,	1, 0},
	{"DL",		html_dl,	2, 0},
	{"DT",		html_dt,	1, 1},
	{"EM",		html_italic,	0, 0},
	{"FIXED",	html_fixed,	0, 0},
	{"FONT",	html_font,	0, 0},
	{"FORM",	html_form,	1, 0},
	{"FRAME",	html_frame,	1, 1},
	{"FRAMESET",	html_frameset,	1, 0},
	{"H1",		html_h1,	2, 2},
	{"H2",		html_h2,	2, 2},
	{"H3",		html_h3,	2, 2},
	{"H4",		html_h4,	2, 2},
	{"H5",		html_h5,	2, 2},
	{"H6",		html_h6,	2, 2},
	/* {"HEAD",	html_skip,	0, 0}, */
	{"HR",		html_hr,	2, 1},
	{"I",		html_italic,	0, 0},
	{"IFRAME",	html_iframe,	1, 1},
	{"IMG",		html_img,	0, 1},
	{"INPUT",	html_input,	0, 1},
	{"LI",		html_li,	1, 3},
	{"LINK",	html_link,	1, 1},
	{"LISTING",	html_pre,	2, 0},
	{"MENU",	html_ul,	2, 0},
	{"NOFRAMES",	html_noframes,	0, 0},
	{"OL",		html_ol,	2, 0},
	{"OPTION",	html_option,	1, 1},
	{"P",		html_p,		2, 2},
	{"PRE",		html_pre,	2, 0},
	{"Q",		html_italic,	0, 0},
	{"S",		html_underline,	0, 0},
	{"SCRIPT",	html_skip,	0, 0},
	{"SELECT",	html_select,	0, 0},
	{"SPAN",	html_span,	0, 0},
	{"STRIKE",	html_underline,	0, 0},
	{"STRONG",	html_bold,	0, 0},
	{"STYLE",	html_skip,	0, 0},
	{"SUB",		html_subscript, 0, 0},
	{"SUP",		html_superscript,0,0},
	{"TABLE",	html_table,	2, 0},
	{"TD",		html_td,	0, 0},
	{"TEXTAREA",	html_textarea,	0, 1},
	{"TH",		html_th,	0, 0},
	{"TITLE",	html_title,	0, 0},
	{"TR",		html_tr,	1, 0},
	{"U",		html_underline,	0, 0},
	{"UL",		html_ul,	2, 0},
	{"XMP",		html_xmp,	2, 0},
	{NULL,		NULL, 0, 0},
};

unsigned char *
skip_comment(unsigned char *html, unsigned char *eof)
{
	int comm = html + 4 <= eof && html[2] == '-' && html[3] == '-';

	html += comm ? 4 : 2;
	while (html < eof) {
		if (!comm && html[0] == '>') return html + 1;
		if (comm && html + 2 <= eof && html[0] == '-' && html[1] == '-') {
			html += 2;
			while (html < eof && *html == '-') html++;
			while (html < eof && WHITECHAR(*html)) html++;
			if (html >= eof) return eof;
			if (*html == '>') return html + 1;
			continue;
		}
		html++;
	}
	return eof;
}

static void
process_head(unsigned char *head)
{
	unsigned char *refresh, *url;

	refresh = parse_http_header(head, "Refresh", NULL);
	if (refresh) {
		url = parse_http_header_param(refresh, "URL");
		if (url) {
			unsigned char *saved_url = url;
			/* Extraction of refresh time. */
			unsigned long seconds;

			errno = 0;
			seconds = strtoul(refresh, NULL, 10);
			if (errno || seconds > 7200) seconds = 0;

			html_focusable(NULL);
			url = join_urls(format.href_base, saved_url);
			put_link_line("Refresh: ", saved_url, url, d_opt->framename);
			special_f(ff, SP_REFRESH, seconds, url);
			mem_free(url);
			mem_free(saved_url);
		}
		mem_free(refresh);
	}
}

#ifndef USE_FASTFIND

static int
compar(const void *a, const void *b)
{
	return strcasecmp(((struct element_info *) a)->name,
			  ((struct element_info *) b)->name);
}

#else

static struct fastfind_info *ff_info_tags;
static struct element_info *internal_pointer;

/* Reset internal list pointer */
void
tags_list_reset(void)
{
	internal_pointer = elements;
}

/* Returns a pointer to a struct that contains
 * current key and data pointers and increment
 * internal pointer.
 * It returns NULL when key is NULL. */
struct fastfind_key_value *
tags_list_next(void)
{
	static struct fastfind_key_value kv;

	if (!internal_pointer->name) return NULL;

	kv.key = internal_pointer->name;
	kv.data = internal_pointer;

	internal_pointer++;

	return &kv;
}

#endif /* USE_FASTFIND */

void
init_tags_lookup(void)
{
#ifdef USE_FASTFIND
	ff_info_tags = fastfind_index(&tags_list_reset, &tags_list_next, 0);
	fastfind_index_compress(ff_info_tags);
#endif
}

void
free_tags_lookup(void)
{
#ifdef USE_FASTFIND
	fastfind_done(ff_info_tags);
#endif
}

void
parse_html(unsigned char *html, unsigned char *eof,
	   void (*put_chars)(void *, unsigned char *, int),
	   void (*line_break)(void *),
	   void (*init)(void *),
	   void *(*special)(void *, int, ...),
	   void *f, unsigned char *head)
{
	/*unsigned char *start = html;*/
	unsigned char *lt;

	putsp = -1;
	line_breax = table_level ? 2 : 1;
	position = 0;
	was_br = 0;
	was_li = 0;
	put_chars_f = put_chars;
	line_break_f = line_break;
	init_f = init;
	special_f = special;
	ff = f;
	eoff = eof;
	if (head) process_head(head);

set_lt:
	put_chars_f = put_chars;
	line_break_f = line_break;
	init_f = init;
	special_f = special;
	ff = f;
	eoff = eof;
	lt = html;
	while (html < eof) {
		struct element_info *ei;
		unsigned char *name, *attr, *end, *prev_html;
		int namelen;
		int inv;
		int dotcounter = 0;

		if (WHITECHAR(*html) && par_format.align != AL_NONE) {
			unsigned char *h = html;

#if 0
			if (putsp == -1) {
				while (html < eof && WHITECHAR(*html)) html++;
				goto set_lt;
			}
			putsp = 0;
#endif
			while (h < eof && WHITECHAR(*h)) h++;
			if (h + 1 < eof && h[0] == '<' && h[1] == '/') {
				if (!parse_element(h, eof, &name, &namelen, &attr, &end)) {
					put_chrs(lt, html - lt, put_chars, f);
					lt = html = h;
					putsp = 1;
					goto element;
				}
			}
			html++;
			if (!(position + (html - lt - 1))) goto skip_w; /* ??? */
			if (*(html - 1) == ' ') {
				/* BIG performance win; not sure if it doesn't cause any bug */
				if (html < eof && !WHITECHAR(*html)) continue;
				put_chrs(lt, html - lt, put_chars, f);
			} else {
				put_chrs(lt, html - 1 - lt, put_chars, f);
				put_chrs(" ", 1, put_chars, f);
			}

skip_w:
			while (html < eof && WHITECHAR(*html)) html++;
			/*putsp = -1;*/
			goto set_lt;

put_sp:
			put_chrs(" ", 1, put_chars, f);
			/*putsp = -1;*/
		}

		if (par_format.align == AL_NONE) {
			putsp = 0;
			if (*html == ASCII_TAB) {
				put_chrs(lt, html - lt, put_chars, f);
				put_chrs("        ", 8 - (position % 8), put_chars, f);
				html++;
				goto set_lt;
			} else if (*html == ASCII_CR || *html == ASCII_LF) {
				put_chrs(lt, html - lt, put_chars, f);

next_break:
				if (*html == ASCII_CR && html < eof - 1
				    && html[1] == ASCII_LF)
					html++;
				ln_break(1, line_break, f);
				html++;
				if (*html == ASCII_CR || *html == ASCII_LF) {
					line_breax = 0;
					goto next_break;
				}
				goto set_lt;
			}
		}

		while (*html < ' ') {
			/*if (putsp == 1) goto put_sp;
			putsp = 0;*/
			if (html - lt) put_chrs(lt, html - lt, put_chars, f);
			dotcounter++;
			html++; lt = html;
			if (*html >= ' ' || WHITECHAR(*html) || html >= eof) {
				unsigned char *dots = mem_alloc(dotcounter);

				if (dots) {
					memset(dots, '.', dotcounter);
					put_chrs(dots, dotcounter, put_chars, f);
					mem_free(dots);
				}
				goto set_lt;
			}
		}

		if (html + 2 <= eof && html[0] == '<' && (html[1] == '!' || html[1] == '?') && !d_opt->plain) {
			/*if (putsp == 1) goto put_sp;
			putsp = 0;*/
			put_chrs(lt, html - lt, put_chars, f);
			html = skip_comment(html, eof);
			goto set_lt;
		}

		if (*html != '<' || d_opt->plain == 1 || parse_element(html, eof, &name, &namelen, &attr, &end)) {
			/*if (putsp == 1) goto put_sp;
			putsp = 0;*/
			html++;
			continue;
		}

element:
		inv = *name == '/'; name += inv; namelen -= inv;
		if (!inv && putsp == 1 && !html_top.invisible) goto put_sp;
		put_chrs(lt, html - lt, put_chars, f);
		if (par_format.align != AL_NONE) if (!inv && !putsp) {
			unsigned char *ee = end;
			unsigned char *nm;

			while (!parse_element(ee, eof, &nm, NULL, NULL, &ee))
				if (*nm == '/') goto ng;
			if (ee < eof && WHITECHAR(*ee)) {
				/*putsp = -1;*/
				put_chrs(" ", 1, put_chars, f);
			}

ng:;
		}

		prev_html = html;
		html = end;
#if 0
		for (ei = elements; ei->name; ei++) {
			if (ei->name &&
			   (strlen(ei->name) != namelen || strncasecmp(ei->name, name, namelen)))
				continue;
#endif

#ifndef USE_FASTFIND
		{
			struct element_info elem;
			unsigned char tmp;

			tmp = name[namelen];
			name[namelen] = '\0';

			elem.name = name;
			ei = bsearch(&elem, elements, NUMBER_OF_TAGS, sizeof(struct element_info), compar);
			name[namelen] = tmp;
		}
#else
		ei = (struct element_info *) fastfind_search(name, namelen, ff_info_tags);
#endif
		while (ei) { /* This exists just to be able to conviently break; out. */
			if (!inv) {
				unsigned char *a;

				if (d_opt->plain) {
					put_chrs("<", 1, put_chars, f);
					html = prev_html + 1;
					break;
				}
				ln_break(ei->linebreak, line_break, f);
				if (ei->name && ((a = get_attr_val(attr, "id")))) {
					special(f, SP_TAG, a);
					mem_free(a);
				}
				if (!html_top.invisible) {
					int ali = (par_format.align == AL_NONE);
					struct par_attrib pa = par_format;

					if (ei->func == html_table && d_opt->tables && table_level < HTML_MAX_TABLE_LEVEL) {
						format_table(attr, html, eof, &html, f);
						ln_break(2, line_break, f);
						goto set_lt;
					}
					if (ei->func == html_select) {
						if (!do_html_select(attr, html, eof, &html, f))
							goto set_lt;
					}
					if (ei->func == html_textarea) {
						do_html_textarea(attr, html, eof, &html, f);
						goto set_lt;
					}
					if (ei->nopair == 2 || ei->nopair == 3) {
						struct html_element *e;

						if (ei->nopair == 2) {
							foreach (e, html_stack) {
								if (e->dontkill) break;
								if (e->linebreak || !ei->linebreak) break;
							}
						} else foreach (e, html_stack) {
							if (e->linebreak && !ei->linebreak) break;
							if (e->dontkill) break;
							if (e->namelen == namelen && !strncasecmp(e->name, name, e->namelen)) break;
						}
						if (e->namelen == namelen && !strncasecmp(e->name, name, e->namelen)) {
							while (e->prev != (void *)&html_stack) kill_html_stack_item(e->prev);
							if (e->dontkill != 2) kill_html_stack_item(e);
						}
					}
					if (ei->nopair != 1) {
						html_stack_dup();
						html_top.name = name;
						html_top.namelen = namelen;
						html_top.options = attr;
						html_top.linebreak = ei->linebreak;
					}
					if (ei->func) ei->func(attr);
					if (ei->func != html_br) was_br = 0;
					if (ali) par_format = pa;
				}
			} else {
				struct html_element *e, *elt;
				int lnb = 0;
				int xxx = 0;

				if (d_opt->plain) {
					if (ei->func == html_xmp) d_opt->plain = 0;
					else break;
				}
				was_br = 0;
				if (ei->nopair == 1 || ei->nopair == 3) break;
				/*debug_stack();*/
				foreach (e, html_stack) {
					if (e->linebreak && !ei->linebreak && ei->name) xxx = 1;
					if (e->namelen != namelen || strncasecmp(e->name, name, e->namelen)) {
						if (e->dontkill)
							break;
						else
							continue;
					}
					if (xxx) {
						kill_html_stack_item(e);
						break;
					}
					for (elt = e; elt != (void *)&html_stack; elt = elt->prev)
						if (elt->linebreak > lnb) lnb = elt->linebreak;
					ln_break(lnb, line_break, f);
					while (e->prev != (void *)&html_stack) kill_html_stack_item(e->prev);
					kill_html_stack_item(e);
					break;
				}
				/*debug_stack();*/
			}
			goto set_lt;
		}
		goto set_lt; /* unreachable */
	}

	put_chrs(lt, html - lt, put_chars, f);
	ln_break(1, line_break, f);
	putsp = -1;
	position = 0;
	/*line_breax = 1;*/
	was_br = 0;
}

static int
look_for_map(unsigned char **pos, unsigned char *eof, unsigned char *tag)
{
	unsigned char *al, *attr, *name;
	int namelen;

	while (*pos < eof && **pos != '<') {
		(*pos)++;
	}

	if (*pos >= eof) return 0;

	if (*pos + 2 <= eof && ((*pos)[1] == '!' || (*pos)[1] == '?')) {
		*pos = skip_comment(*pos, eof);
		return 1;
	}

	if (parse_element(*pos, eof, &name, &namelen, &attr, pos)) {
		(*pos)++;
		return 1;
	}

	if (namelen != 3 || strncasecmp(name, "MAP", 3)) return 1;

	if (tag && *tag) {
		al = get_attr_val(attr, "name");
		if (!al) return 1;

		if (strcasecmp(al, tag)) {
			mem_free(al);
			return 1;
		}

		mem_free(al);
	}

	return 0;
}

static int
look_for_tag(unsigned char **pos, unsigned char *eof,
	     unsigned char *name, int namelen, unsigned char **label)
{
	unsigned char *pos2;
	struct string str;

	if (!init_string(&str)) {
		/* Is this the right way to bail out? --jonas */
		*pos = eof;
		return 0;
	}

	pos2 = *pos;
	while (pos2 < eof && *pos2 != '<') {
		pos2++;
	}

	if (pos2 >= eof) {
		done_string(&str);
		*pos = eof;
		return 0;
	}
	if (pos2 - *pos)
		add_bytes_to_string(&str, *pos, pos2 - *pos);
	*label = str.source;

	*pos = pos2;

	if (*pos + 2 <= eof && ((*pos)[1] == '!' || (*pos)[1] == '?')) {
		*pos = skip_comment(*pos, eof);
		return 1;
	}

	if (parse_element(*pos, eof, NULL, NULL, NULL, &pos2)) return 1;

	if (!((namelen == 1 && !strncasecmp(name, "A", 1)) ||
	      (namelen == 2 && !strncasecmp(name, "/A", 2)) ||
	      (namelen == 3 && !strncasecmp(name, "MAP", 3)) ||
	      (namelen == 4 && !strncasecmp(name, "/MAP", 4)) ||
	      (namelen == 4 && !strncasecmp(name, "AREA", 4)) ||
	      (namelen == 5 && !strncasecmp(name, "/AREA", 5)))) {
		*pos = pos2;
		return 1;
	}

	return 0;
}

static int
look_for_link(unsigned char **pos, unsigned char *eof,
	      unsigned char *tag, struct menu_item **menu,
	      struct memory_list **ml, unsigned char *href_base,
	      unsigned char *target_base, struct conv_table *ct)
{
	unsigned char *attr, *label, *href, *name, *target;
	struct link_def *ld;
	struct menu_item *nm;
	int nmenu = 0;
	int namelen;
	int i;

	while (*pos < eof && **pos != '<') {
		(*pos)++;
	}

	if (*pos >= eof) return 0;

	if (*pos + 2 <= eof && ((*pos)[1] == '!' || (*pos)[1] == '?')) {
		*pos = skip_comment(*pos, eof);
		return 1;
	}

	if (parse_element(*pos, eof, &name, &namelen, &attr, pos)) {
		(*pos)++;
		return 1;
	}

	if (namelen == 1 && !strncasecmp(name, "A", 1)) {
		while (look_for_tag(pos, eof, name, namelen, &label));

		if (*pos >= eof) return 0;

	} else if (namelen == 4 && !strncasecmp(name, "AREA", 4)) {
		unsigned char *alt = get_attr_val(attr, "alt");

		if (alt) {
			label = convert_string(ct, alt, strlen(alt), CSM_DEFAULT);
			mem_free(alt);
		} else {
			label = NULL;
		}

	} else if (namelen == 4 && !strncasecmp(name, "/MAP", 4)) {
		/* This is the only successful return from here! */
		add_to_ml(ml, *menu, NULL);
		return 0;

	} else {
		return 1;
	}

	target = get_target(attr);
	if (!target) target = target_base ? stracpy(target_base) : NULL;
	if (!target) target = stracpy("");
	if (!target) {
		if (label) mem_free(label);
		return 1;
	}

	ld = mem_alloc(sizeof(struct link_def));
	if (!ld) {
		if (label) mem_free(label);
		mem_free(target);
		return 1;
	}

	href = get_url_val(attr, "href");
	if (!href) {
		if (label) mem_free(label);
		mem_free(target);
		mem_free(ld);
		return 1;
	}


	ld->link = join_urls(href_base, href);
	mem_free(href);
	if (!ld->link) {
		if (label) mem_free(label);
		mem_free(target);
		mem_free(ld);
		return 1;
	}


	ld->target = target;
	for (i = 0; i < nmenu; i++) {
		struct link_def *ll = (*menu)[i].data;

		if (!strcmp(ll->link, ld->link) &&
		    !strcmp(ll->target, ld->target)) {
			mem_free(ld->link);
			mem_free(ld->target);
			mem_free(ld);
			if (label) mem_free(label);
			return 1;
		}
	}

	if (label) {
		clr_spaces(label);

		if (!*label) {
			mem_free(label);
			label = NULL;
		}
	}

	if (!label) {
		label = stracpy(ld->link);
		if (!label) {
			mem_free(target);
			mem_free(ld->link);
			mem_free(ld);
			return 1;
		}
	}

	nm = mem_realloc(*menu, (nmenu + 2) * sizeof(struct menu_item));
	if (nm) {
		*menu = nm;
		memset(&nm[nmenu], 0, 2 * sizeof(struct menu_item));
		nm[nmenu].text = label;
		nm[nmenu].rtext = "";
		nm[nmenu].func = (menu_func) map_selected;
		nm[nmenu].data = ld;
		nm[nmenu].no_intl = 1;
		nm[++nmenu].text = NULL;
	}

	add_to_ml(ml, ld, ld->link, ld->target, label, NULL);

	return 1;
}

int
get_image_map(unsigned char *head, unsigned char *pos, unsigned char *eof,
	      unsigned char *tag, struct menu_item **menu,
	      struct memory_list **ml, unsigned char *href_base,
	      unsigned char *target_base, int to, int def, int hdef)
{
	struct conv_table *ct;
	struct string hd;

	if (!init_string(&hd)) return -1;

	if (head) add_to_string(&hd, head);
	scan_http_equiv(pos, eof, &hd, NULL);
	ct = get_convert_table(hd.source, to, def, NULL, NULL, hdef);
	done_string(&hd);

	*menu = mem_calloc(1, sizeof(struct menu_item));
	if (!*menu) return -1;

	while (look_for_map(&pos, eof, tag));

	if (pos >= eof) {
		mem_free(*menu);
		return -1;
	}

	*ml = NULL;

	while (look_for_link(&pos, eof, tag, menu, ml,
			     href_base, target_base, ct));

	if (pos >= eof) {
		freeml(*ml);
		mem_free(*menu);
		return -1;
	}

	return 0;
}

void
scan_http_equiv(unsigned char *s, unsigned char *eof, struct string *head,
		struct string *title)
{
	unsigned char *name, *attr, *he, *c;
	int namelen;

	if (title && !init_string(title)) return;

	add_char_to_string(head, '\n');

se:
	while (s < eof && *s != '<') {
sp:
		s++;
	}
	if (s >= eof) return;
	if (s + 2 <= eof && (s[1] == '!' || s[1] == '?')) {
		s = skip_comment(s, eof);
		goto se;
	}
	if (parse_element(s, eof, &name, &namelen, &attr, &s)) goto sp;

ps:
	if (namelen == 4 && !strncasecmp(name, "HEAD", 4)) goto se;
	if (namelen == 5 && !strncasecmp(name, "/HEAD", 5)) return;
	if (namelen == 4 && !strncasecmp(name, "BODY", 4)) return;
	if (title && !title->length && namelen == 5 && !strncasecmp(name, "TITLE", 5)) {
		unsigned char *s1;

xse:
		s1 = s;
		while (s < eof && *s != '<') {
xsp:
			s++;
		}
		if (s - s1)
			add_bytes_to_string(title, s1, s - s1);
		if (s >= eof) goto se;
		if (s + 2 <= eof && (s[1] == '!' || s[1] == '?')) {
			s = skip_comment(s, eof);
			goto xse;
		}
		if (parse_element(s, eof, &name, &namelen, &attr, &s)) goto xsp;
		clr_spaces(title->source);
		goto ps;
	}
	if (namelen != 4 || strncasecmp(name, "META", 4)) goto se;

	he = get_attr_val(attr, "charset");
	if (he) {
		add_to_string(head, "Charset: ");
		add_to_string(head, he);
		mem_free(he);
	}

	he = get_attr_val(attr, "http-equiv");
	if (!he) goto se;

	add_to_string(head, he);

	c = get_attr_val(attr, "content");
	if (c) {
		add_to_string(head, ": ");
		add_to_string(head, c);
	        mem_free(c);
	}

	mem_free(he);
	add_to_string(head, "\r\n");
	goto se;
}
