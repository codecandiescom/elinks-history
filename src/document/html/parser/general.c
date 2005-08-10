/* General element handlers */
/* $Id: general.c,v 1.16 2005/08/10 14:01:17 witekfl Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* strcasestr() */
#endif

#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

#include "elinks.h"

#include "config/options.h"
#include "document/css/apply.h"
#include "document/html/frames.h"
#include "document/html/parser/general.h"
#include "document/html/parser/link.h"
#include "document/html/parser/stack.h"
#include "document/html/parser/parse.h"
#include "document/html/parser.h"
#include "document/options.h"
#include "intl/charsets.h"
#include "protocol/uri.h"
#include "terminal/draw.h"
#include "util/align.h"
#include "util/box.h"
#include "util/color.h"
#include "util/conv.h"
#include "util/error.h"
#include "util/memdebug.h"
#include "util/memory.h"
#include "util/string.h"

/* Unsafe macros */
#include "document/html/internal.h"


void
html_span(struct html_context *html_context, unsigned char *a)
{
}

void
html_bold(struct html_context *html_context, unsigned char *a)
{
	format.style.attr |= AT_BOLD;
}

void
html_italic(struct html_context *html_context, unsigned char *a)
{
	format.style.attr |= AT_ITALIC;
}

void
html_underline(struct html_context *html_context, unsigned char *a)
{
	format.style.attr |= AT_UNDERLINE;
}

void
html_fixed(struct html_context *html_context, unsigned char *a)
{
	format.style.attr |= AT_FIXED;
}

void
html_subscript(struct html_context *html_context, unsigned char *a)
{
	format.style.attr |= AT_SUBSCRIPT;
}

void
html_superscript(struct html_context *html_context, unsigned char *a)
{
	format.style.attr |= AT_SUPERSCRIPT;
}

void
html_font(struct html_context *html_context, unsigned char *a)
{
	unsigned char *al = get_attr_val(a, "size", html_context->options);

	if (al) {
		int p = 0;
		unsigned s;
		unsigned char *nn = al;
		unsigned char *end;

		if (*al == '+') p = 1, nn++;
		else if (*al == '-') p = -1, nn++;

		errno = 0;
		s = strtoul(nn, (char **) &end, 10);
		if (!errno && *nn && !*end) {
			if (s > 7) s = 7;
			if (!p) format.fontsize = s;
			else format.fontsize += p * s;
			if (format.fontsize < 1) format.fontsize = 1;
			else if (format.fontsize > 7) format.fontsize = 7;
		}
		mem_free(al);
	}
	get_color(html_context, a, "color", &format.style.fg);
}

void
html_body(struct html_context *html_context, unsigned char *a)
{
	get_color(html_context, a, "text", &format.style.fg);
	get_color(html_context, a, "link", &format.clink);
	get_color(html_context, a, "vlink", &format.vlink);

	if (get_bgcolor(html_context, a, &format.style.bg) != -1)
		html_context->was_body_background = 1;

	html_context->was_body = 1; /* this will be used by "meta inside body" */
	html_apply_canvas_bgcolor(html_context);
}

void
html_apply_canvas_bgcolor(struct html_context *html_context)
{
#ifdef CONFIG_CSS
	/* If there are any CSS twaks regarding bgcolor, make sure we will get
	 * it _and_ prefer it over bgcolor attribute. */
	if (html_context->options->css_enable)
		css_apply(html_context, &html_top, &html_context->css_styles,
		          &html_context->stack);
#endif

	if (par_format.bgcolor != format.style.bg) {
		/* Modify the root HTML element - format_html_part() will take
		 * this from there. */
		struct html_element *e = html_context->stack.prev;

		html_context->was_body_background = 1;
		e->parattr.bgcolor = e->attr.style.bg = par_format.bgcolor = format.style.bg;
	}

	if (html_context->has_link_lines
	    && par_format.bgcolor != get_opt_color("document.colors.background")
	    && !search_html_stack(html_context, "BODY")) {
		html_context->special_f(html_context, SP_COLOR_LINK_LINES);
	}
}

void
html_script(struct html_context *html_context, unsigned char *a)
{
#ifdef CONFIG_ECMASCRIPT
	/* We did everything (even possibly html_skip()) in do_html_script(). */
#else
	html_skip(html_context, a);
#endif
}

void
html_style(struct html_context *html_context, unsigned char *a)
{
	html_skip(html_context, a);
}

void
html_html(struct html_context *html_context, unsigned char *a)
{
	/* This is here just to get CSS stuff applied. */

	/* Modify the root HTML element - format_html_part() will take
	 * this from there. */
	struct html_element *e = html_context->stack.prev;

	if (par_format.bgcolor != format.style.bg)
		e->parattr.bgcolor = e->attr.style.bg = par_format.bgcolor = format.style.bg;
}

void
html_head(struct html_context *html_context, unsigned char *a)
{
	/* This makes sure it gets to the stack and helps tame down unclosed
	 * <title>. */
}

void
html_meta(struct html_context *html_context, unsigned char *a)
{
	/* html_handle_body_meta() does all the work. */
}

/* Handles meta tags in the HTML body. */
void
html_handle_body_meta(struct html_context *html_context, unsigned char *meta,
		      unsigned char *eof)
{
	struct string head;

	if (!init_string(&head)) return;

	/* scan_http_equiv() requires that the from-pointer points before
	 * "META", so use a '- 1' here. */ 
	scan_http_equiv(meta - 1, eof, &head, NULL, html_context->options);
	process_head(html_context, head.source);
	done_string(&head);
}

void
html_title(struct html_context *html_context, unsigned char *a)
{
	html_top.invisible = 1;
	html_top.type = ELEMENT_WEAK;
}

void
html_center(struct html_context *html_context, unsigned char *a)
{
	par_format.align = ALIGN_CENTER;
	if (!html_context->table_level)
		par_format.leftmargin = par_format.rightmargin = 0;
}

void
html_linebrk(struct html_context *html_context, unsigned char *a)
{
	unsigned char *al = get_attr_val(a, "align", html_context->options);

	if (al) {
		if (!strcasecmp(al, "left")) par_format.align = ALIGN_LEFT;
		else if (!strcasecmp(al, "right")) par_format.align = ALIGN_RIGHT;
		else if (!strcasecmp(al, "center")) {
			par_format.align = ALIGN_CENTER;
			if (!html_context->table_level)
				par_format.leftmargin = par_format.rightmargin = 0;
		} else if (!strcasecmp(al, "justify")) par_format.align = ALIGN_JUSTIFY;
		mem_free(al);
	}
}

void
html_br(struct html_context *html_context, unsigned char *a)
{
	html_linebrk(html_context, a);
	if (html_context->was_br)
		ln_break(html_context, 2);
	else
		html_context->was_br = 1;
}

void
html_p(struct html_context *html_context, unsigned char *a)
{
	int_lower_bound(&par_format.leftmargin, html_context->margin);
	int_lower_bound(&par_format.rightmargin, html_context->margin);
	/*par_format.align = ALIGN_LEFT;*/
	html_linebrk(html_context, a);
}

void
html_address(struct html_context *html_context, unsigned char *a)
{
	par_format.leftmargin++;
	par_format.align = ALIGN_LEFT;
}

void
html_blockquote(struct html_context *html_context, unsigned char *a)
{
	par_format.leftmargin += 2;
	par_format.align = ALIGN_LEFT;
}

void
html_h(int h, unsigned char *a,
       enum format_align default_align, struct html_context *html_context)
{
	if (!par_format.align) par_format.align = default_align;
	html_linebrk(html_context, a);

	h -= 2;
	if (h < 0) h = 0;

	switch (par_format.align) {
		case ALIGN_LEFT:
			par_format.leftmargin = h * 2;
			par_format.rightmargin = 0;
			break;
		case ALIGN_RIGHT:
			par_format.leftmargin = 0;
			par_format.rightmargin = h * 2;
			break;
		case ALIGN_CENTER:
			par_format.leftmargin = par_format.rightmargin = 0;
			break;
		case ALIGN_JUSTIFY:
			par_format.leftmargin = par_format.rightmargin = h * 2;
			break;
	}
}

void
html_h1(struct html_context *html_context, unsigned char *a)
{
	format.style.attr |= AT_BOLD;
	html_h(1, a, ALIGN_CENTER, html_context);
}

void
html_h2(struct html_context *html_context, unsigned char *a)
{
	html_h(2, a, ALIGN_LEFT, html_context);
}

void
html_h3(struct html_context *html_context, unsigned char *a)
{
	html_h(3, a, ALIGN_LEFT, html_context);
}

void
html_h4(struct html_context *html_context, unsigned char *a)
{
	html_h(4, a, ALIGN_LEFT, html_context);
}

void
html_h5(struct html_context *html_context, unsigned char *a)
{
	html_h(5, a, ALIGN_LEFT, html_context);
}

void
html_h6(struct html_context *html_context, unsigned char *a)
{
	html_h(6, a, ALIGN_LEFT, html_context);
}

void
html_pre(struct html_context *html_context, unsigned char *a)
{
	format.style.attr |= AT_PREFORMATTED;
	par_format.leftmargin = (par_format.leftmargin > 1);
	par_format.rightmargin = 0;
}

void
html_xmp(struct html_context *html_context, unsigned char *a)
{
	html_context->was_xmp = 1;
	html_pre(html_context, a);
}

void
html_hr(struct html_context *html_context, unsigned char *a)
{
	int i/* = par_format.width - 10*/;
	unsigned char r = (unsigned char) BORDER_DHLINE;
	int q = get_num(a, "size", html_context->options);

	if (q >= 0 && q < 2) r = (unsigned char) BORDER_SHLINE;
	html_stack_dup(html_context, ELEMENT_KILLABLE);
	par_format.align = ALIGN_CENTER;
	mem_free_set(&format.link, NULL);
	format.form = NULL;
	html_linebrk(html_context, a);
	if (par_format.align == ALIGN_JUSTIFY) par_format.align = ALIGN_CENTER;
	par_format.leftmargin = par_format.rightmargin = html_context->margin;

	i = get_width(a, "width", 1, html_context);
	if (i == -1) i = get_html_max_width();
	format.style.attr = AT_GRAPHICS;
	html_context->special_f(html_context, SP_NOWRAP, 1);
	while (i-- > 0) {
		put_chrs(html_context, &r, 1);
	}
	html_context->special_f(html_context, SP_NOWRAP, 0);
	ln_break(html_context, 2);
	kill_html_stack_item(html_context, &html_top);
}

void
html_table(struct html_context *html_context, unsigned char *a)
{
	par_format.leftmargin = par_format.rightmargin = html_context->margin;
	par_format.align = ALIGN_LEFT;
	html_linebrk(html_context, a);
	format.style.attr = 0;
}

void
html_tt(struct html_context *html_context, unsigned char *a)
{
}

void
html_tr(struct html_context *html_context, unsigned char *a)
{
	html_linebrk(html_context, a);
}

void
html_th(struct html_context *html_context, unsigned char *a)
{
	/*html_linebrk(html_context, a);*/
	kill_html_stack_until(html_context, 1,
	                      "TD", "TH", "", "TR", "TABLE", NULL);
	format.style.attr |= AT_BOLD;
	put_chrs(html_context, " ", 1);
}

void
html_td(struct html_context *html_context, unsigned char *a)
{
	/*html_linebrk(html_context, a);*/
	kill_html_stack_until(html_context, 1,
	                      "TD", "TH", "", "TR", "TABLE", NULL);
	format.style.attr &= ~AT_BOLD;
	put_chrs(html_context, " ", 1);
}

void
html_base(struct html_context *html_context, unsigned char *a)
{
	unsigned char *al;

	al = get_url_val(a, "href", html_context->options);
	if (al) {
		unsigned char *base = join_urls(html_context->base_href, al);
		struct uri *uri = base ? get_uri(base, 0) : NULL;

		mem_free(al);
		mem_free_if(base);

		if (uri) {
			done_uri(html_context->base_href);
			html_context->base_href = uri;
		}
	}

	al = get_target(html_context->options, a);
	if (al) mem_free_set(&html_context->base_target, al);
}

void
html_ul(struct html_context *html_context, unsigned char *a)
{
	unsigned char *al;

	/* dump_html_stack(html_context); */
	par_format.list_level++;
	par_format.list_number = 0;
	par_format.flags = P_STAR;

	al = get_attr_val(a, "type", html_context->options);
	if (al) {
		if (!strcasecmp(al, "disc") || !strcasecmp(al, "circle"))
			par_format.flags = P_O;
		else if (!strcasecmp(al, "square"))
			par_format.flags = P_PLUS;
		mem_free(al);
	}
	par_format.leftmargin += 2 + (par_format.list_level > 1);
	if (!html_context->table_level)
		int_upper_bound(&par_format.leftmargin, par_format.width / 2);

	par_format.align = ALIGN_LEFT;
	html_top.type = ELEMENT_DONT_KILL;
}

void
html_ol(struct html_context *html_context, unsigned char *a)
{
	unsigned char *al;
	int st;

	par_format.list_level++;
	st = get_num(a, "start", html_context->options);
	if (st == -1) st = 1;
	par_format.list_number = st;
	par_format.flags = P_NUMBER;

	al = get_attr_val(a, "type", html_context->options);
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
	if (!html_context->table_level)
		int_upper_bound(&par_format.leftmargin, par_format.width / 2);

	par_format.align = ALIGN_LEFT;
	html_top.type = ELEMENT_DONT_KILL;
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

void
html_li(struct html_context *html_context, unsigned char *a)
{
	/* When handling the code <li><li> @was_li will be 1 and it means we
	 * have to insert a line break since no list item content has done it
	 * for us. */
	if (html_context->was_li) {
		html_context->line_breax = 0;
		ln_break(html_context, 1);
	}

	/*kill_html_stack_until(html_context, 0
	                      "", "UL", "OL", NULL);*/
	if (!par_format.list_number) {
		unsigned char x[7] = "*&nbsp;";
		int t = par_format.flags & P_LISTMASK;

		if (t == P_O) x[0] = 'o';
		if (t == P_PLUS) x[0] = '+';
		put_chrs(html_context, x, 7);
		par_format.leftmargin += 2;
		par_format.align = ALIGN_LEFT;

	} else {
		unsigned char c = 0;
		unsigned char n[32];
		int nlen;
		int t = par_format.flags & P_LISTMASK;
		int s = get_num(a, "value", html_context->options);

		if (s != -1) par_format.list_number = s;

		if (t == P_ALPHA || t == P_alpha) {
			put_chrs(html_context, "&nbsp;", 6);
			c = 1;
			n[0] = par_format.list_number
			       ? (par_format.list_number - 1) % 26
			         + (t == P_ALPHA ? 'A' : 'a')
			       : 0;
			n[1] = 0;

		} else if (t == P_ROMAN || t == P_roman) {
			roman(n, par_format.list_number);
			if (t == P_ROMAN) {
				unsigned char *x;

				for (x = n; *x; x++) *x = toupper(*x);
			}

		} else {
			if (par_format.list_number < 10) {
				put_chrs(html_context, "&nbsp;", 6);
				c = 1;
			}

			ulongcat(n, NULL, par_format.list_number, (sizeof(n) - 1), 0);
		}

		nlen = strlen(n);
		put_chrs(html_context, n, nlen);
		put_chrs(html_context, ".&nbsp;", 7);
		par_format.leftmargin += nlen + c + 2;
		par_format.align = ALIGN_LEFT;

		{
			struct html_element *element;

			element = search_html_stack(html_context, "ol");
			if (element)
				element->parattr.list_number = par_format.list_number + 1;
		}

		par_format.list_number = 0;
	}

	html_context->putsp = HTML_SPACE_SUPPRESS;
	html_context->line_breax = 2;
	html_context->was_li = 1;
}

void
html_dl(struct html_context *html_context, unsigned char *a)
{
	par_format.flags &= ~P_COMPACT;
	if (has_attr(a, "compact", html_context->options))
		par_format.flags |= P_COMPACT;
	if (par_format.list_level) par_format.leftmargin += 5;
	par_format.list_level++;
	par_format.list_number = 0;
	par_format.align = ALIGN_LEFT;
	par_format.dd_margin = par_format.leftmargin;
	html_top.type = ELEMENT_DONT_KILL;
	if (!(par_format.flags & P_COMPACT)) {
		ln_break(html_context, 2);
		html_top.linebreak = 2;
	}
}

void
html_dt(struct html_context *html_context, unsigned char *a)
{
	kill_html_stack_until(html_context, 0, "", "DL", NULL);
	par_format.align = ALIGN_LEFT;
	par_format.leftmargin = par_format.dd_margin;
	if (!(par_format.flags & P_COMPACT)
	    && !has_attr(a, "compact", html_context->options))
		ln_break(html_context, 2);
}

void
html_dd(struct html_context *html_context, unsigned char *a)
{
	kill_html_stack_until(html_context, 0, "", "DL", NULL);

	par_format.leftmargin = par_format.dd_margin + 3;

	if (!html_context->table_level) {
		par_format.leftmargin += 5;
		int_upper_bound(&par_format.leftmargin, par_format.width / 2);
	}
	par_format.align = ALIGN_LEFT;
}



void
html_noframes(struct html_context *html_context, unsigned char *a)
{
	struct html_element *element;

	if (!html_context->options->frames) return;

	element = search_html_stack(html_context, "frameset");
	if (element && !element->frameset) return;

	html_skip(html_context, a);
}

void
html_frame(struct html_context *html_context, unsigned char *a)
{
	unsigned char *name, *src, *url;

	src = get_url_val(a, "src", html_context->options);
	if (!src) {
		url = stracpy("about:blank");
	} else {
		url = join_urls(html_context->base_href, src);
		mem_free(src);
	}
	if (!url) return;

	name = get_attr_val(a, "name", html_context->options);
	if (!name) {
		name = stracpy(url);
	} else if (!name[0]) {
		/* When name doesn't have a value */
		mem_free(name);
		name = stracpy(url);
	}
	if (!name) return;

	if (!html_context->options->frames || !html_top.frameset) {
		html_focusable(html_context, a);
		put_link_line("Frame: ", name, url, "", html_context);

	} else {
		if (html_context->special_f(html_context, SP_USED, NULL)) {
			html_context->special_f(html_context, SP_FRAME,
					       html_top.frameset, name, url);
		}
	}

	mem_free(name);
	mem_free(url);
}

void
html_frameset(struct html_context *html_context, unsigned char *a)
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
	if (search_html_stack(html_context, "BODY")
	    || !html_context->options->frames
	    || !html_context->special_f(html_context, SP_USED, NULL))
		return;

	cols = get_attr_val(a, "cols", html_context->options);
	if (!cols) {
		cols = stracpy("100%");
		if (!cols) return;
	}

	rows = get_attr_val(a, "rows", html_context->options);
	if (!rows) {
		rows = stracpy("100%");
	       	if (!rows) {
			mem_free(cols);
			return;
		}
	}

	if (!html_top.frameset) {
		width = html_context->options->box.width;
		height = html_context->options->box.height;
		html_context->options->needs_height = 1;
	} else {
		struct frameset_desc *frameset_desc = html_top.frameset;
		int offset;

		if (frameset_desc->box.y >= frameset_desc->box.height)
			goto free_and_return;
		offset = frameset_desc->box.x
			 + frameset_desc->box.y * frameset_desc->box.width;
		width = frameset_desc->frame_desc[offset].width;
		height = frameset_desc->frame_desc[offset].height;
	}

	fp.width = fp.height = NULL;

	parse_frame_widths(cols, width, HTML_FRAME_CHAR_WIDTH,
			   &fp.width, &fp.x);
	parse_frame_widths(rows, height, HTML_FRAME_CHAR_HEIGHT,
			   &fp.height, &fp.y);

	fp.parent = html_top.frameset;
	if (fp.x && fp.y) {
		html_top.frameset = html_context->special_f(html_context, SP_FRAMESET, &fp);
	}
	mem_free_if(fp.width);
	mem_free_if(fp.height);

free_and_return:
	mem_free(cols);
	mem_free(rows);
}

void
html_noscript(struct html_context *html_context, unsigned char *a)
{
	/* We shouldn't throw <noscript> away until our ECMAScript support is
	 * halfway decent. */
#if 0
// #ifdef CONFIG_ECMASCRIPT
	if (get_opt_bool("ecmascript.enable"))
		html_skip(html_context, a);
#endif
}
