/* HTML parser */
/* $Id: parser.c,v 1.401 2004/04/23 22:28:40 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* strcasestr() */
#endif

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "main.h"
#include "bfu/listmenu.h"
#include "bfu/menu.h"
#include "config/options.h"
#include "config/kbdbind.h"
#include "document/css/apply.h"
#include "document/css/css.h"
#include "document/css/parser.h"
#include "document/css/stylesheet.h"
#include "document/html/frames.h"
#include "document/html/parser.h"
#include "document/html/renderer.h"
#include "document/html/tables.h"
#include "globhist/globhist.h"
#include "intl/charsets.h"
#include "osdep/ascii.h"
#include "protocol/http/header.h"
#include "protocol/uri.h"
#include "sched/session.h"
#include "sched/task.h"
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
#include "viewer/text/form.h"
#include "viewer/text/link.h"
#include "viewer/text/view.h"

/* Unsafe macros */
#include "document/html/internal.h"

/* TODO: This needs rewrite. Yes, no kidding. */

struct form {
	unsigned char *action;
	unsigned char *target;
	int method;
	int num;
};

#define NULL_STRUCT_FORM { NULL, NULL, 0, 0 }

INIT_LIST_HEAD(html_stack);

#define end_of_tag(c) ((c) == '>' || (c) == '<')

static inline int
atchr(register unsigned char c)
{
	return (c < 127 && (c > '>' || (c > ' ' && c != '=' && !end_of_tag(c))));
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
#define next_char() if (++e == eof) return -1;

	assert(e && eof);
	if (e >= eof || *e != '<') return -1;

	next_char();
	if (name) *name = e;

	if (*e == '/') next_char();
	if (!isA(*e)) return -1;

	while (isA(*e)) next_char();

	if (!isspace(*e) && !end_of_tag(*e) && *e != '/' && *e != ':')
		return -1;

	if (name && namelen) *namelen = e - *name;

	while (isspace(*e) || *e == '/' || *e == ':') next_char();

	/* Skip bad attribute */
	while (!atchr(*e) && !end_of_tag(*e) && !isspace(*e)) next_char();

	if (attr) *attr = e;

next_attr:
	while (isspace(*e)) next_char();

	/* Skip bad attribute */
	while (!atchr(*e) && !end_of_tag(*e) && !isspace(*e)) next_char();

	if (end_of_tag(*e)) goto end;

	while (atchr(*e)) next_char();
	while (isspace(*e)) next_char();

	if (*e != '=') {
		if (end_of_tag(*e)) goto end;
		goto next_attr;
	}
	next_char();

	while (isspace(*e)) next_char();

	if (IS_QUOTE(*e)) {
		unsigned char quote = *e;

quoted_value:
		next_char();
		while (*e != quote) next_char();
		next_char();
		if (*e == quote) goto quoted_value;
	} else {
		while (!isspace(*e) && !end_of_tag(*e)) next_char();
	}

	while (isspace(*e)) next_char();

	if (!end_of_tag(*e)) goto next_attr;

end:
	if (end) *end = e + (*e == '>');

	return 0;
}

#define realloc_chrs(x, l) mem_align_alloc(x, l, (l) + 1, unsigned char, 0xFF)

#define add_chr(s, l, c)						\
	do {								\
		if (!realloc_chrs(&(s), l)) return NULL;		\
		(s)[(l)++] = (c);					\
	} while (0)

/* Parses html element attributes. */
/* - e is attr pointer previously get from parse_element,
 * DON'T PASS HERE ANY OTHER VALUE!!!
 * - name is searched attribute */
/* Returns allocated string containing the attribute, or NULL on unsuccess.
 * If @test_only is different from zero then we only test for existence of
 * an attribute of that @name. In that mode it returns NULL if attribute
 * was not found, and a pointer to start of the attribute if it was found.
 * If @eat_nl is zero, newline and tabs chars are replaced by spaces
 * in returned value, else these chars are skipped. */
static inline unsigned char *
get_attr_val_(register unsigned char *e, unsigned char *name, int test_only,
	      int eat_nl)
{
	unsigned char *n;
	unsigned char *name_start;
	unsigned char *attr = NULL;
	int attrlen = 0;
	int found;

next_attr:
	while (isspace(*e)) e++;
	if (end_of_tag(*e) || !atchr(*e)) goto parse_error;
	n = name;
	name_start = e;

	while (atchr(*n) && atchr(*e) && upcase(*e) == upcase(*n)) e++, n++;
	found = !*n && !atchr(*e);

	if (found && test_only) return name_start;

	while (atchr(*e)) e++;
	while (isspace(*e)) e++;
	if (*e != '=') {
		if (found) goto found_endattr;
		goto next_attr;
	}
	e++;
	while (isspace(*e)) e++;

	if (found) {
		if (!IS_QUOTE(*e)) {
			while (!isspace(*e) && !end_of_tag(*e)) {
				if (!*e) goto parse_error;
				add_chr(attr, attrlen, *e);
				e++;
			}
		} else {
			unsigned char quote = *e;

parse_quoted_value:
			while (*(++e) != quote) {
				if (*e == ASCII_CR) continue;
				if (!*e) goto parse_error;
				if (*e != ASCII_TAB && *e != ASCII_LF)
					add_chr(attr, attrlen, *e);
				else if (!eat_nl)
					add_chr(attr, attrlen, ' ');
			}
			e++;
			if (*e == quote) {
				add_chr(attr, attrlen, *e);
				goto parse_quoted_value;
			}
		}

found_endattr:
		add_chr(attr, attrlen, '\0');
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
			while (!isspace(*e) && !end_of_tag(*e)) {
				if (!*e) goto parse_error;
				e++;
			}
		} else {
			unsigned char quote = *e;

			do {
				while (*(++e) != quote)
					if (!*e) goto parse_error;
				e++;
			} while (*e == quote);
		}
	}

	goto next_attr;

parse_error:
	mem_free_if(attr);
	return NULL;
}

#undef add_chr

unsigned char *
get_attr_val(register unsigned char *e, unsigned char *name)
{
	return get_attr_val_(e, name, 0, 0);
}

static unsigned char *
get_url_val(unsigned char *e, unsigned char *name)
{
	return get_attr_val_(e, name, 0, 1);
}

int
has_attr(unsigned char *e, unsigned char *name)
{
	return !!get_attr_val_(e, name, 1, 0);
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

	if (!use_document_fg_colors(global_doc_opts))
		return -1;

	at = get_attr_val(a, c);
	if (!at) return -1;

	r = decode_color(at, strlen(at), rgb);
	mem_free(at);

	return r;
}

int
get_bgcolor(unsigned char *a, color_t *rgb)
{
	if (!use_document_bg_colors(global_doc_opts))
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
			v = stracpy(global_doc_opts->framename);
		}
	}

	return v;
}

#if 0
static void
dump_html_stack()
{
	struct html_element *element;

	foreach (element, html_stack) {
		DBG(":%p:%d:%.*s", element->name, element->namelen,
				element->namelen, element->name);
	}
	WDBG("Did you enjoy it?");
}
#endif

static struct html_element *
search_html_stack(char *name)
{
	struct html_element *element;
	int namelen;

	assert(name && *name);
	namelen = strlen(name);

#if 0	/* Debug code. Please keep. */
	dump_html_stack();
#endif

	foreach (element, html_stack) {
		if (element == &html_top)
			continue; /* skip the top element */
		if (strlcasecmp(element->name, element->namelen, name, namelen))
			continue;
		return element;
	}

	return NULL;
}

static void
kill_html_stack_item(struct html_element *e)
{
	assert(e);
	if_assert_failed return;
	assertm((void *)e != &html_stack, "trying to free bad html element");
	if_assert_failed return;
	assertm(e->type != ELEMENT_IMMORTAL, "trying to kill unkillable element");
	if_assert_failed return;

	mem_free_if(e->attr.link);
	mem_free_if(e->attr.target);
	mem_free_if(e->attr.image);
	mem_free_if(e->attr.title);
	mem_free_if(e->attr.href_base);
	mem_free_if(e->attr.target_base);
	mem_free_if(e->attr.select);
	del_from_list(e);
	mem_free(e);
#if 0
	if (list_empty(html_stack) || !html_stack.next) {
		DBG("killing last element");
	}
#endif
}

static inline void
kill_elem(unsigned char *e)
{
	if (!strlcasecmp(html_top.name, html_top.namelen, e, -1))
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
		printf("\" : %d", e->type);
		printf("\n");
	}
	printf("%c", 7);
	fflush(stdout);
	sleep(1);
}
#endif

static void
html_stack_dup(enum html_element_type type)
{
	struct html_element *e;
	struct html_element *ep = html_stack.next;

	assertm(ep && (void *)ep != &html_stack, "html stack empty");
	if_assert_failed return;

	e = mem_alloc(sizeof(struct html_element));
	if (!e) return;
	memcpy(e, ep, sizeof(struct html_element));
	if (ep->attr.link) e->attr.link = stracpy(ep->attr.link);
	if (ep->attr.target) e->attr.target = stracpy(ep->attr.target);
	if (ep->attr.image) e->attr.image = stracpy(ep->attr.image);
	if (ep->attr.title) e->attr.title = stracpy(ep->attr.title);
	if (ep->attr.href_base) e->attr.href_base = stracpy(ep->attr.href_base);
	if (ep->attr.target_base) e->attr.target_base = stracpy(ep->attr.target_base);
	if (ep->attr.select) e->attr.select = stracpy(ep->attr.select);
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
	e->type = type;
	add_to_list(html_stack, e);
}

void *ff;
void (*put_chars_f)(void *, unsigned char *, int);
void (*line_break_f)(void *);
void *(*special_f)(void *, enum html_special_type, ...);

static unsigned char *eoff;
static unsigned char *eofff;
static unsigned char *startf;

static int line_breax;
static int position;
static int putsp;

static int was_br;
static int was_li;
static int was_xmp;
static int has_link_lines;

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
		if (isspace(start[0])) start++, len--;
		putsp = 0;
	}
	if (!len) {
		putsp = -1;
		if (par_format.align == AL_NONE) putsp = 0;
		return;
	}
	if (isspace(start[len - 1])) putsp = -1;
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

				if (!strlcasecmp(e->name, e->namelen, s, slen)) {
					if (!sk) {
						if (e->type < ELEMENT_KILLABLE) break;
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

		if (e->type < ELEMENT_KILLABLE
		    || (!strlcasecmp(e->name, e->namelen, "TABLE", 5)))
			break;

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

/* Extract numerical value of attribute @name.
 * It will return a positive integer value on success,
 * or -1 on error. */
int
get_num(unsigned char *a, unsigned char *name)
{
	unsigned char *al = get_attr_val(a, name);
	int result = -1;

	if (al) {
		unsigned char *end;
		long num;

		errno = 0;
		num = strtol(al, (char **)&end, 10);
		if (!errno && !*end && num >= 0 && num <= MAXINT)
			result = (int) num;

		mem_free(al);
	}

	return result;
}

static int
parse_width(unsigned char *w, int trunc)
{
	unsigned char *end;
	int p = 0;
	int s;
	int l;
	int width;

	while (isspace(*w)) w++;
	for (l = 0; w[l] && w[l] != ','; l++);

	while (l && isspace(w[l - 1])) l--;
	if (!l) return -1;

	if (w[l - 1] == '%') l--, p = 1;

	while (l && isspace(w[l - 1])) l--;
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

void
set_fragment_identifier(unsigned char *attr_name, unsigned char *attr)
{
	unsigned char *id_attr = get_attr_val(attr_name, attr);

	if (id_attr) {
		special_f(ff, SP_TAG, id_attr);
		mem_free(id_attr);
	}
}

void
add_fragment_identifier(void *part, unsigned char *attr)
{
	special_f(part, SP_TAG, attr);
}

static void
import_css_stylesheet(struct css_stylesheet *css, unsigned char *url, int len)
{
	unsigned char *import_url;

	if (!global_doc_opts->css_enable
	    || !global_doc_opts->css_import)
		return;

	url = memacpy(url, len);
	if (!url) return;

	/* HTML <head> urls should already be fine but we can.t detect them. */
	import_url = join_urls(format.href_base, url);
	mem_free(url);

	if (!import_url) return;

	/* Request the imported stylesheet as part of the document ... */
	special_f(ff, SP_STYLESHEET, import_url);

	/* ... and then attempt to import from the cache. */
	import_css(css, import_url);

	mem_free(import_url);
}

static struct form form = NULL_STRUCT_FORM;
static INIT_CSS_STYLESHEET(css_styles, import_css_stylesheet);

static unsigned char *last_form_tag;
static unsigned char *last_form_attr;
static unsigned char *last_input_tag;

static unsigned char *object_src;

static void
put_link_line(unsigned char *prefix, unsigned char *linkname,
	      unsigned char *link, unsigned char *target)
{
	has_link_lines = 1;
	html_stack_dup(ELEMENT_KILLABLE);
	ln_break(1, line_break_f, ff);
	mem_free_set(&format.link, NULL);
	mem_free_set(&format.target, NULL);
	mem_free_set(&format.title, NULL);
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
	unsigned char *href;

	href = get_url_val(a, "href");
	if (href) {
		unsigned char *target;

		mem_free_set(&format.link,
				join_urls(format.href_base, trim_chars(href, ' ', 0)));

		mem_free(href);

		target = get_target(a);
		if (target) {
			mem_free_set(&format.target, target);
		} else {
			mem_free_set(&format.target, stracpy(format.target_base));
		}
#ifdef CONFIG_GLOBHIST
		if (get_global_history_item(format.link))
			format.fg = format.vlink;
		else
#endif
			format.fg = format.clink;

		mem_free_set(&format.title, get_attr_val(a, "title"));

		html_focusable(a);

	} else {
		kill_html_stack_item(&html_top);
	}

	set_fragment_identifier(a, "name");
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
		html_stack_dup(ELEMENT_KILLABLE);
		mem_free_if(format.link);
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
		mem_free_if(al);
		if (!global_doc_opts->images && !format.link) return;

		add_brackets = 1;

		if (usemap) {
			al = stracpy("USEMAP");
		} else if (ismap) {
			al = stracpy("ISMAP");
		} else {
			unsigned char *src = NULL;
			int max_real_len;
			int max_len;

			src = null_or_stracpy(object_src);
			if (!src) src = get_url_val(a, "src");

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
					al = memacpy(start, len);
					if (!al) return;
				}
			} else {
				al = stracpy("IMG");
			}

			mem_free_if(src);
		}
	}

	mem_free_set(&format.image, NULL);
	mem_free_set(&format.title, NULL);

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

		if (!get_opt_bool("document.browse.images.show_any_as_links")) {
			ismap = 0;
			goto show_al;
		}

		if ((s = null_or_stracpy(object_src))
		    || (s = get_url_val(a, "src"))
		    || (s = get_url_val(a, "dynsrc"))) {
			format.image = join_urls(format.href_base, s);
			mem_free(s);
		}

		format.title = get_attr_val(a, "title");

		if (ismap) {
			unsigned char *h;

			html_stack_dup(ELEMENT_KILLABLE);
			h = stracpy(format.link);
			if (h) {
				add_to_strn(&h, "?0,0");
				mem_free(format.link);
				format.link = h;
			}
		}
show_al:
		/* This is not 100% appropriate for <img>, but well, accepting
		 * accesskey and tabindex near <img> is just our little
		 * extension to the standart. After all, it makes sense. */
		html_focusable(a);

		put_chrs(al, strlen(al), put_chars_f, ff);
		if (ismap) kill_html_stack_item(&html_top);
		/* Anything below must take care of properly handling the
		 * show_any_as_links variable being off! */
	}
	mem_free_set(&format.image, NULL);
	mem_free_set(&format.title, NULL);
	mem_free_if(al);
	if (usemap) kill_html_stack_item(&html_top);
	/*put_chrs(" ", 1, put_chars_f, ff);*/
}

static void
html_body(unsigned char *a)
{
	get_color(a, "text", &format.fg);
	get_color(a, "link", &format.clink);
	get_color(a, "vlink", &format.vlink);

	get_bgcolor(a, &format.bg);
	/* If there are any CSS twaks regarding bgcolor, make sure we will get
	 * it _and_ prefer it over bgcolor attribute. */
	if (global_doc_opts->css_enable)
		css_apply(&html_top, &css_styles);

	if (par_format.bgcolor != format.bg) {
		/* Modify the root HTML element - format_html_part() will take
		 * this from there. */
		struct html_element *e = html_stack.prev;

		e->parattr.bgcolor = e->attr.bg = par_format.bgcolor = format.bg;
	}

	if (has_link_lines
	    && par_format.bgcolor
	    && !search_html_stack("BODY")) {
		special_f(ff, SP_COLOR_LINK_LINES);
	}
}

static void
html_skip(unsigned char *a)
{
	html_top.invisible = 1;
	html_top.type = ELEMENT_DONT_KILL;
}

static void
html_style(unsigned char *a)
{
	html_skip(a);
}

static void
html_head(unsigned char *a)
{
	/* This makes sure it gets to the stack and helps tame down unclosed
	 * <title>. */
}

static void
html_title(unsigned char *a)
{
	html_top.invisible = 1;
	html_top.type = ELEMENT_WEAK;
}

static void
html_center(unsigned char *a)
{
	par_format.align = AL_CENTER;
	if (!table_level)
		par_format.leftmargin = par_format.rightmargin = 0;
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
			if (!table_level)
				par_format.leftmargin = par_format.rightmargin = 0;
		} else if (!strcasecmp(al, "justify")) par_format.align = AL_BLOCK;
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
	if (!par_format.align) par_format.align = default_align;
	html_linebrk(a);

	h -= 2;
	if (h < 0) h = 0;

	switch (par_format.align) {
		case AL_LEFT:
			par_format.leftmargin = h * 2;
			par_format.rightmargin = 0;
			break;
		case AL_RIGHT:
			par_format.leftmargin = 0;
			par_format.rightmargin = h * 2;
			break;
		case AL_CENTER:
			par_format.leftmargin = par_format.rightmargin = 0;
			break;
		case AL_BLOCK:
			par_format.leftmargin = par_format.rightmargin = h * 2;
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
	par_format.leftmargin = (par_format.leftmargin > 1);
	par_format.rightmargin = 0;
}

static void
html_xmp(unsigned char *a)
{
	was_xmp = 1;
	html_pre(a);
}

static void
html_hr(unsigned char *a)
{
	int i/* = par_format.width - 10*/;
	unsigned char r = (unsigned char)BORDER_DHLINE;
	int q = get_num(a, "size");

	if (q >= 0 && q < 2) r = (unsigned char)BORDER_SHLINE;
	html_stack_dup(ELEMENT_KILLABLE);
	par_format.align = AL_CENTER;
	mem_free_set(&format.link, NULL);
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
	par_format.leftmargin = par_format.rightmargin = margin;
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
		mem_free_set(&format.href_base,
				join_urls(((struct html_element *)html_stack.prev)->attr.href_base, al));
		mem_free(al);
	}

	al = get_target(a);
	if (al) mem_free_set(&format.target_base, al);
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
	if (!table_level)
		int_upper_bound(&par_format.leftmargin, par_format.width / 2);

	par_format.align = AL_LEFT;
	html_top.type = ELEMENT_DONT_KILL;
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
	html_top.type = ELEMENT_DONT_KILL;
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
		int t = par_format.flags & P_LISTMASK;

		if (t == P_O) x[0] = 'o';
		if (t == P_PLUS) x[0] = '+';
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
	html_top.type = ELEMENT_DONT_KILL;
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
			int len = get_no_post_url_length(form->action);

			form->action[len] = '\0';

			/* We have to do following for GET method, because we would end
			 * up with two '?' otherwise. */
			if (form->method == FM_GET) {
				unsigned char *ch = strchr(form->action, '?');
				if (ch) *ch = '\0';
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

	mem_free_if(form.action);
	mem_free_if(form.target);
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
	if (strlcasecmp(name, namelen, "FORM", 4)) goto se;
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
	fc->action = null_or_stracpy(form.action);
	fc->name = get_attr_val(a, "name");

	fc->default_value = get_attr_val(a, "value");
	if (!fc->default_value && fc->type == FC_SUBMIT) fc->default_value = stracpy("Submit");
	if (!fc->default_value && fc->type == FC_RESET) fc->default_value = stracpy("Reset");
	if (!fc->default_value) fc->default_value = stracpy("");

	fc->ro = has_attr(a, "disabled") ? 2 : has_attr(a, "readonly") ? 1 : 0;
	if (fc->type == FC_IMAGE) fc->alt = get_attr_val(a, "alt");
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
	fc->action = null_or_stracpy(form.action);
	fc->target = null_or_stracpy(form.target);
	fc->name = get_attr_val(a, "name");

	if (fc->type != FC_FILE) fc->default_value = get_attr_val(a, "value");
	if (!fc->default_value && fc->type == FC_CHECKBOX) fc->default_value = stracpy("on");
	if (!fc->default_value && fc->type == FC_SUBMIT) fc->default_value = stracpy("Submit");
	if (!fc->default_value && fc->type == FC_RESET) fc->default_value = stracpy("Reset");
	if (!fc->default_value) fc->default_value = stracpy("");

	fc->size = get_num(a, "size");
	if (fc->size == -1) fc->size = global_doc_opts->default_form_input_size;
	fc->size++;
	if (fc->size > global_doc_opts->width) fc->size = global_doc_opts->width;
	fc->maxlength = get_num(a, "maxlength");
	if (fc->maxlength == -1) fc->maxlength = MAXINT;
	if (fc->type == FC_CHECKBOX || fc->type == FC_RADIO) fc->default_state = has_attr(a, "checked");
	fc->ro = has_attr(a, "disabled") ? 2 : has_attr(a, "readonly") ? 1 : 0;
	if (fc->type == FC_IMAGE) fc->alt = get_attr_val(a, "alt");
	if (fc->type == FC_HIDDEN) goto hid;

	put_chrs(" ", 1, put_chars_f, ff);
	html_stack_dup(ELEMENT_KILLABLE);
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
			mem_free_set(&format.image, NULL);
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
			if (fc->default_value)
				put_chrs(fc->default_value, strlen(fc->default_value), put_chars_f, ff);
			put_chrs("&nbsp;]", 7, put_chars_f, ff);
			break;
		case FC_TEXTAREA:
		case FC_SELECT:
		case FC_HIDDEN:
			INTERNAL("bad control type");
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
	html_top.type = ELEMENT_DONT_KILL;
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
			INTERNAL("parse element failed");
			val = str.source;
			goto x;
		}

rrrr:
		while (p < eoff && isspace(*p)) p++;
		while (p < eoff && !isspace(*p) && *p != '<') {

pppp:
			add_char_to_string(&str, *p), p++;
		}

		r = p;
		val = str.source; /* Has to be before the possible 'goto x' */

		while (r < eoff && isspace(*r)) r++;
		if (r >= eoff) goto x;
		if (r - 2 <= eoff && (r[1] == '!' || r[1] == '?')) {
			p = skip_comment(r, eoff);
			goto rrrr;
		}
		if (parse_element(r, eoff, &name, &namelen, NULL, &p)) goto pppp;
		if (strlcasecmp(name, namelen, "OPTION", 6)
		    && strlcasecmp(name, namelen, "/OPTION", 7)
		    && strlcasecmp(name, namelen, "SELECT", 6)
		    && strlcasecmp(name, namelen, "/SELECT", 7)
		    && strlcasecmp(name, namelen, "OPTGROUP", 8)
		    && strlcasecmp(name, namelen, "/OPTGROUP", 9))
			goto rrrr;
	}

x:
	fc->form_num = last_form_tag - startf;
	fc->ctrl_num = a - last_form_tag;
	fc->position = a - startf;
	fc->method = form.method;
	fc->action = null_or_stracpy(form.action);
	fc->type = FC_CHECKBOX;
	fc->name = null_or_stracpy(format.select);
	fc->default_value = val;
	fc->default_state = has_attr(a, "selected");
	fc->ro = format.select_disabled;
	if (has_attr(a, "disabled")) fc->ro = 2;
	put_chrs(" ", 1, put_chars_f, ff);
	html_stack_dup(ELEMENT_KILLABLE);
	format.form = fc;
	format.attr |= AT_BOLD;
	put_chrs("[ ]", 3, put_chars_f, ff);
	kill_html_stack_item(&html_top);
	put_chrs(" ", 1, put_chars_f, ff);
	special_f(ff, SP_CONTROL, fc);
}

static struct list_menu lnk_menu;

static int
do_html_select(unsigned char *attr, unsigned char *html,
	       unsigned char *eof, unsigned char **end, void *f)
{
	struct conv_table *ct = special_f(f, SP_TABLE, NULL);
	struct form_control *fc;
	struct string lbl = NULL_STRING;
	unsigned char **val, **labels;
	unsigned char *t_name, *t_attr, *en;
	int t_namelen;
	int nnmi = 0;
	int order, preselect, group;
	int i, max_width;

	if (has_attr(attr, "multiple")) return 1;
	find_form_for_input(attr);
	html_focusable(attr);
	val = NULL;
	order = 0, group = 0, preselect = -1;
	init_menu(&lnk_menu);

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
		destroy_menu(&lnk_menu);
		*end = en;
		return 0;
	}

	if (lbl.source) {
		unsigned char *q, *s = en;
		int l = html - en;

		while (l && isspace(s[0])) s++, l--;
		while (l && isspace(s[l-1])) l--;
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

	if (!strlcasecmp(t_name, t_namelen, "/SELECT", 7)) {
		add_select_item(&lnk_menu, &lbl, val, order, nnmi);
		goto end_parse;
	}

	if (!strlcasecmp(t_name, t_namelen, "/OPTION", 7)) {
		add_select_item(&lnk_menu, &lbl, val, order, nnmi);
		goto see;
	}

	if (!strlcasecmp(t_name, t_namelen, "OPTION", 6)) {
		unsigned char *v, *vx;

		add_select_item(&lnk_menu, &lbl, val, order, nnmi);

		if (has_attr(t_attr, "disabled")) goto see;
		if (preselect == -1 && has_attr(t_attr, "selected")) preselect = order;
		v = get_attr_val(t_attr, "value");

		if (!mem_align_alloc(&val, order, order + 1, unsigned char *, 0xFF))
			goto abort;

		val[order++] = v;
		vx = get_attr_val(t_attr, "label");
		if (vx) new_menu_item(&lnk_menu, vx, order - 1, 0);
		if (!v || !vx) {
			init_string(&lbl);
			nnmi = !!vx;
		}
		goto see;
	}

	if (!strlcasecmp(t_name, t_namelen, "OPTGROUP", 8)
	    || !strlcasecmp(t_name, t_namelen, "/OPTGROUP", 9)) {
		add_select_item(&lnk_menu, &lbl, val, order, nnmi);

		if (group) new_menu_item(&lnk_menu, NULL, -1, 0), group = 0;
	}

	if (!strlcasecmp(t_name, t_namelen, "OPTGROUP", 8)) {
		unsigned char *label = get_attr_val(t_attr, "label");

		if (!label) {
			label = stracpy("");
			if (!label) goto see;
		}
		new_menu_item(&lnk_menu, label, -1, 0);
		group = 1;
	}
	goto see;


end_parse:
	*end = en;
	if (!order) goto abort;

	fc = mem_calloc(1, sizeof(struct form_control));
	if (!fc) goto abort;

	labels = mem_calloc(order, sizeof(unsigned char *));
	if (!labels) {
		mem_free(fc);
		goto abort;
	}

	fc->form_num = last_form_tag - startf;
	fc->ctrl_num = attr - last_form_tag;
	fc->position = attr - startf;
	fc->method = form.method;
	fc->action = null_or_stracpy(form.action);
	fc->name = get_attr_val(attr, "name");
	fc->type = FC_SELECT;
	fc->default_state = preselect < 0 ? 0 : preselect;
	fc->default_value = order ? stracpy(val[fc->default_state]) : stracpy("");
	fc->ro = has_attr(attr, "disabled") ? 2 : has_attr(attr, "readonly") ? 1 : 0;
	fc->nvalues = order;
	fc->values = val;
	fc->menu = detach_menu(&lnk_menu);
	fc->labels = labels;

	menu_labels(fc->menu, "", labels);
	put_chrs("[", 1, put_chars_f, f);
	html_stack_dup(ELEMENT_KILLABLE);
	format.form = fc;
	format.attr |= AT_BOLD;

	max_width = 0;
	for (i = 0; i < order; i++) {
		if (!labels[i]) continue;
		int_lower_bound(&max_width, strlen(labels[i]));
	}

	for (i = 0; i < max_width; i++)
		put_chrs("_", 1, put_chars_f, f);

	kill_html_stack_item(&html_top);
	put_chrs("]", 1, put_chars_f, f);
	special_f(ff, SP_CONTROL, fc);

	return 0;
}

static void
html_textarea(unsigned char *a)
{
	INTERNAL("This should be never called");
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
	if (strlcasecmp(t_name, t_namelen, "/TEXTAREA", 9)) goto pp;

	fc = mem_calloc(1, sizeof(struct form_control));
	if (!fc) return;

	fc->form_num = last_form_tag - startf;
	fc->ctrl_num = attr - last_form_tag;
	fc->position = attr - startf;
	fc->method = form.method;
	fc->action = null_or_stracpy(form.action);
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
	if (cols <= 0) cols = global_doc_opts->default_form_input_size;
	cols++; /* Add 1 column, other browsers may have different
		   behavior here (mozilla adds 2) --Zas */
	if (cols > global_doc_opts->width) cols = global_doc_opts->width;
	fc->cols = cols;

	rows = get_num(attr, "rows");
	if (rows <= 0) rows = 1;
	if (rows > global_doc_opts->height) rows = global_doc_opts->height;
	fc->rows = rows;
	global_doc_opts->needs_height = 1;

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

	html_stack_dup(ELEMENT_KILLABLE);
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
html_applet(unsigned char *a)
{
	unsigned char *code, *alt = NULL;

	code = get_url_val(a, "code");
	if (!code) return;

	alt = get_attr_val(a, "alt");
	if (!alt) alt = stracpy("");
	if (!alt) {
		mem_free(code);
		return;
	}

	html_focusable(a);

	if (*alt) {
		put_link_line("Applet: ", alt, code, global_doc_opts->framename);
	} else {
		put_link_line("", "Applet", code, global_doc_opts->framename);
	}

	mem_free(alt);
	mem_free(code);
}

static void
html_iframe(unsigned char *a)
{
	unsigned char *name, *url = NULL;

	url = null_or_stracpy(object_src);
	if (!url) url = get_url_val(a, "src");
	if (!url) return;

	name = get_attr_val(a, "name");
	if (!name) name = get_attr_val(a, "id");
	if (!name) name = stracpy("");
	if (!name) {
		mem_free(url);
		return;
	}

	html_focusable(a);

	if (*name) {
		put_link_line("IFrame: ", name, url, global_doc_opts->framename);
	} else {
		put_link_line("", "IFrame", url, global_doc_opts->framename);
	}

	mem_free(name);
	mem_free(url);
}

static void
html_object(unsigned char *a)
{
	unsigned char *type, *url;

	/* This is just some dirty wrapper. We emulate various things through
	 * this, which is anyway in the spirit of <object> element, unifying
	 * <img> and <iframe> etc. */

	url = get_url_val(a, "data");
	if (!url) return;

	type = get_attr_val(a, "type");
	if (!type) { mem_free(url); return; }

	if (!strncasecmp(type, "text/", 5)) {
		/* We will just emulate <iframe>. */
		object_src = url;
		html_iframe(a);
		object_src = NULL;
		html_skip(a);

	} else if (!strncasecmp(type, "image/", 6)) {
		/* <img> emulation. */
		/* TODO: Use the enclosed text as 'alt' attribute. */
		object_src = url;
		html_img(a);
		object_src = NULL;
	}

	mem_free(type);
	mem_free(url);
}

static void
html_noframes(unsigned char *a)
{
	struct html_element *element;

	if (!global_doc_opts->frames) return;

	element = search_html_stack("frameset");
	if (element && !element->frameset) return;

	html_skip(a);
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

	if (!global_doc_opts->frames || !html_top.frameset) {
		html_focusable(a);
		put_link_line("Frame: ", name, url, "");

	} else {
		if (special_f(ff, SP_USED, NULL)) {
			special_f(ff, SP_FRAME, html_top.frameset, name, url);
		}
	}

	mem_free(name);
	mem_free(url);
}

static void
html_frameset(unsigned char *a)
{
	struct frameset_param fp;
	unsigned char *cols, *rows;
	int width, height;

	/* XXX: This is still not 100% correct. We should also ignore the
	 * frameset when we encountered anything 3v1l (read as: non-whitespace
	 * text/element/anything) in the document outside of <head>. Well, this
	 * is still better than nothing and it should heal up the security
	 * concerns at least because sane sites should enclose the documents in
	 * <body> elements ;-). See also bug 171. --pasky */
	if (search_html_stack("BODY")
	    || !global_doc_opts->frames || !special_f(ff, SP_USED, NULL))
		return;

	cols = get_attr_val(a, "cols");
	if (!cols) {
		cols = stracpy("100%");
		if (!cols) return;
	}

	rows = get_attr_val(a, "rows");
	if (!rows) {
		rows = stracpy("100%");
	       	if (!rows) {
			mem_free(cols);
			return;
		}
	}

	if (!html_top.frameset) {
		width = global_doc_opts->width;
		height = global_doc_opts->height;
		global_doc_opts->needs_height = 1;
	} else {
		struct frameset_desc *frameset_desc = html_top.frameset;
		int offset;

		if (frameset_desc->y >= frameset_desc->height)
			goto free_and_return;
		offset = frameset_desc->x
			 + frameset_desc->y * frameset_desc->width;
		width = frameset_desc->frame_desc[offset].width;
		height = frameset_desc->frame_desc[offset].height;
	}

	fp.width = fp.height = NULL;

	parse_frame_widths(cols, width, HTML_FRAME_CHAR_WIDTH,
			   &fp.width, &fp.x);
	parse_frame_widths(rows, height, HTML_FRAME_CHAR_HEIGHT,
			   &fp.height, &fp.y);

	fp.parent = html_top.frameset;
	if (fp.x && fp.y) html_top.frameset = special_f(ff, SP_FRAMESET, &fp);
	mem_free_if(fp.width);
	mem_free_if(fp.height);

free_and_return:
	mem_free(cols);
	mem_free(rows);
}

/* Link types:

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

Some more were added, like top. --Zas */

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
/* XXX: Keep the (really really ;) default name first */
static struct lt_default_name lt_names[] = {
	{ LT_START, "start" },
	{ LT_START, "top" },
	{ LT_START, "home" },
	{ LT_PARENT, "parent" },
	{ LT_PARENT, "up" },
	{ LT_NEXT, "next" },
	{ LT_PREV, "previous" },
	{ LT_PREV, "prev" },
	{ LT_CONTENTS, "contents" },
	{ LT_CONTENTS, "toc" },
	{ LT_INDEX, "index" },
	{ LT_GLOSSARY, "glossary" },
	{ LT_CHAPTER, "chapter" },
	{ LT_SECTION, "section" },
	{ LT_SUBSECTION, "subsection" },
	{ LT_SUBSECTION, "child" },
	{ LT_SUBSECTION, "sibling" },
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
	{ LT_AUTHOR, "made" },
	{ LT_AUTHOR, "owner" },
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

	mem_free_if(link->content_type);
	mem_free_if(link->media);
	mem_free_if(link->href);
	mem_free_if(link->hreflang);
	mem_free_if(link->title);
	mem_free_if(link->lang);
	mem_free_if(link->name);

	memset(link, 0, sizeof(struct hlink));
}

/* Parse a link and return results in @link.
 * It tries to identify known types. */
static int
html_link_parse(unsigned char *a, struct hlink *link)
{
	int i;

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
	for (i = 0; lt_names[i].str; i++)
		if (!strcasecmp(link->name, lt_names[i].str)) {
			link->type = lt_names[i].type;
			return 1;
		}

	if (strcasestr(link->name, "icon") ||
	   (link->content_type && strcasestr(link->content_type, "icon"))) {
		link->type = LT_ICON;

	} else if (strcasestr(link->name, "alternate")) {
		link->type = LT_ALTERNATE;
		if (link->lang)
			link->type = LT_ALTERNATE_LANG;
		else if (strcasestr(link->name, "stylesheet") ||
			 (link->content_type && strcasestr(link->content_type, "css")))
			link->type = LT_ALTERNATE_STYLESHEET;
		else if (link->media)
			link->type = LT_ALTERNATE_MEDIA;
	} else if (link->content_type && strcasestr(link->content_type, "css")) {
		link->type = LT_STYLESHEET;
	}

	return 1;
}

static void
html_link(unsigned char *a)
{
	int link_display = global_doc_opts->meta_link_display;
	unsigned char *name = NULL;
	struct hlink link;
	static unsigned char link_rel_string[] = "Link: ";
	static unsigned char link_rev_string[] = "Reverse link: ";

	if (!link_display) return;
	if (!html_link_parse(a, &link)) return;

	if (link.type == LT_STYLESHEET) {
		import_css_stylesheet(&css_styles, link.href, strlen(link.href));
	}

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

#define NUMBER_OF_TAGS 65

static struct element_info elements[] = {
	{"A",		html_a,		0, 2},
	{"ABBR",	html_italic,	0, 0},
	{"ADDRESS",	html_address,	2, 0},
	{"APPLET",	html_applet,	1, 1},
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
	{"HEAD",	html_head,	0, 0},
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
	{"OBJECT",	html_object,	1, 1},
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
	{"STYLE",	html_style,	0, 0},
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
			while (html < eof && isspace(*html)) html++;
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
			put_link_line("Refresh: ", saved_url, url, global_doc_opts->framename);
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
	ff_info_tags = fastfind_index(&tags_list_reset, &tags_list_next,
				      0, "tags_lookup");
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
	   void *f, unsigned char *head)
{
	/*unsigned char *start = html;*/
	unsigned char *lt;

	putsp = -1;
	line_breax = table_level ? 2 : 1;
	position = 0;
	was_br = 0;
	was_li = 0;
	ff = f;
	eoff = eof;
	if (head) process_head(head);

set_lt:
	ff = f;
	eoff = eof;
	lt = html;
	while (html < eof) {
		struct element_info *ei;
		unsigned char *name, *attr, *end, *prev_html;
		int namelen;
		int inv;
		int dotcounter = 0;

		if (isspace(*html) && par_format.align != AL_NONE) {
			unsigned char *h = html;

#if 0
			if (putsp == -1) {
				while (html < eof && isspace(*html)) html++;
				goto set_lt;
			}
			putsp = 0;
#endif
			while (h < eof && isspace(*h)) h++;
			if (h + 1 < eof && h[0] == '<' && h[1] == '/') {
				if (!parse_element(h, eof, &name, &namelen, &attr, &end)) {
					put_chrs(lt, html - lt, put_chars_f, f);
					lt = html = h;
					putsp = 1;
					goto element;
				}
			}
			html++;
			if (!(position + (html - lt - 1))) goto skip_w; /* ??? */
			if (isspace(*(html - 1))) {
				/* BIG performance win; not sure if it doesn't cause any bug */
				if (html < eof && !isspace(*html)) continue;
				put_chrs(lt, html - lt, put_chars_f, f);
			} else {
				put_chrs(lt, html - 1 - lt, put_chars_f, f);
				put_chrs(" ", 1, put_chars_f, f);
			}

skip_w:
			while (html < eof && isspace(*html)) html++;
			/*putsp = -1;*/
			goto set_lt;

put_sp:
			put_chrs(" ", 1, put_chars_f, f);
			/*putsp = -1;*/
		}

		if (par_format.align == AL_NONE) {
			putsp = 0;
			if (*html == ASCII_TAB) {
				put_chrs(lt, html - lt, put_chars_f, f);
				put_chrs("        ", 8 - (position % 8), put_chars_f, f);
				html++;
				goto set_lt;
			} else if (*html == ASCII_CR || *html == ASCII_LF) {
				put_chrs(lt, html - lt, put_chars_f, f);

next_break:
				if (*html == ASCII_CR && html < eof - 1
				    && html[1] == ASCII_LF)
					html++;
				ln_break(1, line_break_f, f);
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
			if (html - lt) put_chrs(lt, html - lt, put_chars_f, f);
			dotcounter++;
			html++; lt = html;
			if (*html >= ' ' || isspace(*html) || html >= eof) {
				unsigned char *dots = fmem_alloc(dotcounter);

				if (dots) {
					memset(dots, '.', dotcounter);
					put_chrs(dots, dotcounter, put_chars_f, f);
					fmem_free(dots);
				}
				goto set_lt;
			}
		}

		if (html + 2 <= eof && html[0] == '<' && (html[1] == '!' || html[1] == '?') && !was_xmp) {
			/*if (putsp == 1) goto put_sp;
			putsp = 0;*/
			put_chrs(lt, html - lt, put_chars_f, f);
			html = skip_comment(html, eof);
			goto set_lt;
		}

		if (*html != '<' || parse_element(html, eof, &name, &namelen, &attr, &end)) {
			/*if (putsp == 1) goto put_sp;
			putsp = 0;*/
			html++;
			continue;
		}

element:
		inv = *name == '/'; name += inv; namelen -= inv;
		if (!inv && putsp == 1 && !html_top.invisible) goto put_sp;
		put_chrs(lt, html - lt, put_chars_f, f);
		if (par_format.align != AL_NONE && !inv && !putsp) {
			unsigned char *ee = end;
			unsigned char *nm;

			while (!parse_element(ee, eof, &nm, NULL, NULL, &ee))
				if (*nm == '/')
					goto ng;
			if (ee < eof && isspace(*ee)) {
				/*putsp = -1;*/
				put_chrs(" ", 1, put_chars_f, f);
			}

ng:;
		}

		prev_html = html;
		html = end;
#if 0
		for (ei = elements; ei->name; ei++) {
			if (strlcasecmp(ei->name, -1, name, namelen))
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

				if (was_xmp) {
					put_chrs("<", 1, put_chars_f, f);
					html = prev_html + 1;
					break;
				}
				ln_break(ei->linebreak, line_break_f, f);
				if ((a = get_attr_val(attr, "id"))) {
					special_f(f, SP_TAG, a);
					mem_free(a);
				}
				if (html_top.type == ELEMENT_WEAK) {
					kill_html_stack_item(&html_top);
				}
				if (!html_top.invisible) {
					int ali = (par_format.align == AL_NONE);
					struct par_attrib pa = par_format;

					if (ei->func == html_table && global_doc_opts->tables && table_level < HTML_MAX_TABLE_LEVEL) {
						format_table(attr, html, eof, &html, f);
						ln_break(2, line_break_f, f);
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
					if (ei->func == html_style && global_doc_opts->css_enable) {
						css_parse_stylesheet(&css_styles, html, eof);
					}
					if (ei->nopair == 2 || ei->nopair == 3) {
						struct html_element *e;

						if (ei->nopair == 2) {
							foreach (e, html_stack) {
								if (e->type < ELEMENT_KILLABLE) break;
								if (e->linebreak || !ei->linebreak) break;
							}
						} else foreach (e, html_stack) {
							if (e->linebreak && !ei->linebreak) break;
							if (e->type < ELEMENT_KILLABLE) break;
							if (!strlcasecmp(e->name, e->namelen, name, namelen)) break;
						}
						if (!strlcasecmp(e->name, e->namelen, name, namelen)) {
							while (e->prev != (void *)&html_stack) kill_html_stack_item(e->prev);

							if (e->type > ELEMENT_IMMORTAL)
								kill_html_stack_item(e);
						}
					}
					if (ei->nopair != 1) {
						html_stack_dup(ELEMENT_KILLABLE);
						html_top.name = name;
						html_top.namelen = namelen;
						html_top.options = attr;
						html_top.linebreak = ei->linebreak;
					}
					if (html_top.options && global_doc_opts->css_enable) {
						/* XXX: We should apply CSS
						 * otherwise as well, but
						 * that'll need some deeper
						 * changes in order to have
						 * options filled etc. Probably
						 * just calling css_apply()
						 * from more places, since we
						 * usually have nopair set when
						 * we either (1) rescan on your
						 * own from somewhere else (2)
						 * html_stack_dup() in our own
						 * way. --pasky */
						/* Call it now to gain some of
						 * the stuff which might affect
						 * formatting of some elements. */
						css_apply(&html_top, &css_styles);
					}
					if (ei->func) ei->func(attr);
					if (html_top.options && global_doc_opts->css_enable) {
						/* Call it now to override
						 * default colors of the
						 * elements. */
						css_apply(&html_top, &css_styles);
					}
					if (ei->func != html_br) was_br = 0;
					if (ali) par_format = pa;
				}
			} else {
				struct html_element *e, *elt;
				int lnb = 0;
				int xxx = 0;

				if (was_xmp) {
					if (ei->func == html_xmp)
						was_xmp = 0;
					else
						break;
				}

				was_br = 0;
				if (ei->nopair == 1 || ei->nopair == 3) break;
				/*debug_stack();*/
				foreach (e, html_stack) {
					if (e->linebreak && !ei->linebreak) xxx = 1;
					if (strlcasecmp(e->name, e->namelen, name, namelen)) {
						if (e->type < ELEMENT_KILLABLE)
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
					ln_break(lnb, line_break_f, f);
					while (e->prev != (void *)&html_stack) kill_html_stack_item(e->prev);
					kill_html_stack_item(e);
					break;
				}
				/*debug_stack();*/
			}
			goto set_lt;
		}
		goto set_lt;
	}

	put_chrs(lt, html - lt, put_chars_f, f);
	ln_break(1, line_break_f, f);
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

	if (strlcasecmp(name, namelen, "MAP", 3)) return 1;

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

	if (strlcasecmp(name, namelen, "A", 1)
	    && strlcasecmp(name, namelen, "/A", 2)
	    && strlcasecmp(name, namelen, "MAP", 3)
	    && strlcasecmp(name, namelen, "/MAP", 4)
	    && strlcasecmp(name, namelen, "AREA", 4)
	    && strlcasecmp(name, namelen, "/AREA", 5)) {
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
	int nmenu;
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

	if (!strlcasecmp(name, namelen, "A", 1)) {
		while (look_for_tag(pos, eof, name, namelen, &label));

		if (*pos >= eof) return 0;

	} else if (!strlcasecmp(name, namelen, "AREA", 4)) {
		unsigned char *alt = get_attr_val(attr, "alt");

		if (alt) {
			label = convert_string(ct, alt, strlen(alt), CSM_DEFAULT);
			mem_free(alt);
		} else {
			label = NULL;
		}

	} else if (!strlcasecmp(name, namelen, "/MAP", 4)) {
		/* This is the only successful return from here! */
		add_to_ml(ml, *menu, NULL);
		return 0;

	} else {
		return 1;
	}

	target = get_target(attr);
	if (!target) target = null_or_stracpy(target_base);
	if (!target) target = stracpy("");
	if (!target) {
		mem_free_if(label);
		return 1;
	}

	ld = mem_alloc(sizeof(struct link_def));
	if (!ld) {
		mem_free_if(label);
		mem_free(target);
		return 1;
	}

	href = get_url_val(attr, "href");
	if (!href) {
		mem_free_if(label);
		mem_free(target);
		mem_free(ld);
		return 1;
	}


	ld->link = join_urls(href_base, href);
	mem_free(href);
	if (!ld->link) {
		mem_free_if(label);
		mem_free(target);
		mem_free(ld);
		return 1;
	}


	ld->target = target;
	for (nmenu = 0; !mi_is_end_of_menu((*menu)[nmenu]); nmenu++) {
		struct link_def *ll = (*menu)[nmenu].data;

		if (!strcmp(ll->link, ld->link) &&
		    !strcmp(ll->target, ld->target)) {
			mem_free(ld->link);
			mem_free(ld->target);
			mem_free(ld);
			mem_free_if(label);
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
		nm[nmenu].func = (menu_func) map_selected;
		nm[nmenu].data = ld;
		nm[nmenu].flags = NO_INTL;
	}

	add_to_ml(ml, ld, ld->link, ld->target, label, NULL);

	return 1;
}

static void
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
	if (!strlcasecmp(name, namelen, "HEAD", 4)) goto se;
	if (!strlcasecmp(name, namelen, "/HEAD", 5)) return;
	if (!strlcasecmp(name, namelen, "BODY", 4)) return;
	if (title && !title->length && !strlcasecmp(name, namelen, "TITLE", 5)) {
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
	if (strlcasecmp(name, namelen, "META", 4)) goto se;

	he = get_attr_val(attr, "charset");
	if (he) {
		add_to_string(head, "Charset: ");
		add_to_string(head, he);
		mem_free(he);
	}

	he = get_attr_val(attr, "http-equiv");
	if (!he) goto se;

	add_to_string(head, he);
	mem_free(he);

	c = get_attr_val(attr, "content");
	if (c) {
		add_to_string(head, ": ");
		add_to_string(head, c);
	        mem_free(c);
	}

	add_to_string(head, "\r\n");
	goto se;
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



struct html_element *
init_html_parser_state(enum html_element_type type, int align, int margin, int width)
{
	struct html_element *element;

	html_stack_dup(type);
	element = &html_top;

	par_format.align = align;

	if (type <= ELEMENT_IMMORTAL) {
		par_format.leftmargin = margin;
		par_format.rightmargin = margin;
		par_format.width = width;
		par_format.list_level = 0;
		par_format.list_number = 0;
		par_format.dd_margin = 0;
		html_top.namelen = 0;
	}

	return element;
}

void
done_html_parser_state(struct html_element *element)
{
	line_breax = 1;

	while (&html_top != element) {
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

	html_top.type = ELEMENT_KILLABLE;
	kill_html_stack_item(&html_top);

}



void
init_html_parser(unsigned char *url, struct document_options *options,
		 unsigned char *start, unsigned char *end,
		 struct string *head, struct string *title,
		 void (*put_chars)(void *, unsigned char *, int),
		 void (*line_break)(void *),
		 void *(*special)(void *, enum html_special_type, ...))
{
	struct html_element *e;

	assert(url && options);
	if_assert_failed return;
	assertm(list_empty(html_stack), "something on html stack");
	if_assert_failed init_list(html_stack);

	startf = start;
	eofff = end;
	put_chars_f = put_chars;
	line_break_f = line_break;
	special_f = special;
	scan_http_equiv(start, end, head, title);

	e = mem_calloc(1, sizeof(struct html_element));
	if (!e) return;

	add_to_list(html_stack, e);

	format.attr = 0;
	format.fontsize = 3;
	format.link = format.target = format.image = NULL;
	format.select = NULL;
	format.form = NULL;
	format.title = NULL;

	format.fg = options->default_fg;
	format.bg = options->default_bg;
	format.clink = options->default_link;
	format.vlink = options->default_vlink;

	format.href_base = stracpy(url);
	format.target_base = null_or_stracpy(options->framename);

	par_format.align = AL_LEFT;
	par_format.leftmargin = options->margin;
	par_format.rightmargin = options->margin;

	par_format.width = options->width;
	par_format.list_level = par_format.list_number = 0;
	par_format.dd_margin = options->margin;
	par_format.flags = P_NONE;

	par_format.bgcolor = options->default_bg;

	html_top.invisible = 0;
	html_top.name = NULL;
   	html_top.namelen = 0;
	html_top.options = NULL;
	html_top.linebreak = 1;
	html_top.type = ELEMENT_DONT_KILL;

	has_link_lines = 0;
	table_level = 0;
	last_form_tag = NULL;
	last_form_attr = NULL;
	last_input_tag = NULL;

	mirror_css_stylesheet(&css_styles, &default_stylesheet);
}

void
done_html_parser(void)
{
	if (global_doc_opts->css_enable)
		done_css_stylesheet(&css_styles);

	mem_free_set(&form.action, NULL);
	mem_free_set(&form.target, NULL);

	kill_html_stack_item(html_stack.next);

	assertm(list_empty(html_stack),
		"html stack not empty after operation");
	if_assert_failed init_list(html_stack);
}
