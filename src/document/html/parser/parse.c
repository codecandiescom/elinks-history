/* HTML core parser routines */
/* $Id: parse.c,v 1.4 2004/04/24 01:00:18 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "document/css/apply.h"
#include "document/css/parser.h"
#include "document/html/parser/link.h"
#include "document/html/parser/parse.h"
#include "document/html/parser/stack.h"
#include "document/html/parser.h"
#include "document/html/tables.h"
#include "document/options.h"
#include "intl/charsets.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/fastfind.h"
#include "util/memdebug.h"
#include "util/memory.h"
#include "util/string.h"

/* Unsafe macros */
#include "document/html/internal.h"


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

unsigned char *
get_url_val(unsigned char *e, unsigned char *name)
{
	return get_attr_val_(e, name, 0, 1);
}

int
has_attr(unsigned char *e, unsigned char *name)
{
	return !!get_attr_val_(e, name, 1, 0);
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




/* These should be exported properly by specific HTML parser modules
 * implementing them. But for now... */

void html_address(unsigned char *);
void html_base(unsigned char *);
void html_blockquote(unsigned char *);
void html_body(unsigned char *);
void html_bold(unsigned char *);
void html_br(unsigned char *);
void html_button(unsigned char *);
void html_center(unsigned char *);
void html_dd(unsigned char *);
void html_dl(unsigned char *);
void html_dt(unsigned char *);
void html_fixed(unsigned char *);
void html_font(unsigned char *);
void html_form(unsigned char *);
void html_frame(unsigned char *);
void html_frameset(unsigned char *);
void html_h1(unsigned char *);
void html_h2(unsigned char *);
void html_h3(unsigned char *);
void html_h4(unsigned char *);
void html_h5(unsigned char *);
void html_h6(unsigned char *);
void html_head(unsigned char *);
void html_hr(unsigned char *);
void html_input(unsigned char *);
void html_italic(unsigned char *);
void html_li(unsigned char *);
void html_linebrk(unsigned char *);
void html_noframes(unsigned char *);
void html_ol(unsigned char *);
void html_option(unsigned char *);
void html_p(unsigned char *);
void html_pre(unsigned char *);
void html_select(unsigned char *);
void html_skip(unsigned char *);
void html_span(unsigned char *);
void html_style(unsigned char *);
void html_subscript(unsigned char *);
void html_superscript(unsigned char *);
void html_table(unsigned char *);
void html_td(unsigned char *);
void html_textarea(unsigned char *);
void html_th(unsigned char *);
void html_title(unsigned char *);
void html_tr(unsigned char *);
void html_ul(unsigned char *);
void html_underline(unsigned char *);
void html_xmp(unsigned char *);


struct element_info {
	unsigned char *name;
	void (*func)(unsigned char *);

	int linebreak;

	/* 0 - normal pair tags
	 * 1 - normal non-pair tags
	 * 2 - pair tags which cannot be nested (ie. you cannot have <a><a>)
	 * 3 - similiar to 2 but a little stricter, seems to be a
	 *     <li>-specific hack */
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
static void
tags_list_reset(void)
{
	internal_pointer = elements;
}

/* Returns a pointer to a struct that contains
 * current key and data pointers and increment
 * internal pointer.
 * It returns NULL when key is NULL. */
static struct fastfind_key_value *
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
			goto set_lt;

put_sp:
			put_chrs(" ", 1, put_chars_f, f);
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
			put_chrs(lt, html - lt, put_chars_f, f);
			html = skip_comment(html, eof);
			goto set_lt;
		}

		if (*html != '<' || parse_element(html, eof, &name, &namelen, &attr, &end)) {
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

					if (ei->func == html_table && global_doc_opts->tables
					    && table_level < HTML_MAX_TABLE_LEVEL) {
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
							while (e->prev != (void *)&html_stack)
								kill_html_stack_item(e->prev);

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
					if (ei->func == html_xmp) {
						was_xmp = 0;
					} else {
						break;
					}
				}

				was_br = 0;
				if (ei->nopair == 1 || ei->nopair == 3) break;
				/* dump_html_stack(); */
				foreach (e, html_stack) {
					if (e->linebreak && !ei->linebreak) xxx = 1;
					if (strlcasecmp(e->name, e->namelen, name, namelen)) {
						if (e->type < ELEMENT_KILLABLE) {
							break;
						} else {
							continue;
						}
					}
					if (xxx) {
						kill_html_stack_item(e);
						break;
					}
					for (elt = e; elt != (void *)&html_stack; elt = elt->prev)
						if (elt->linebreak > lnb)
							lnb = elt->linebreak;
					ln_break(lnb, line_break_f, f);
					while (e->prev != (void *)&html_stack)
						kill_html_stack_item(e->prev);
					kill_html_stack_item(e);
					break;
				}
				/* dump_html_stack(); */
			}
			goto set_lt;
		}
		goto set_lt;
	}

	put_chrs(lt, html - lt, put_chars_f, f);
	ln_break(1, line_break_f, f);
	putsp = -1;
	position = 0;
	was_br = 0;
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
