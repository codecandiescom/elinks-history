/* HTML parser */
/* $Id: parser.c,v 1.478 2004/07/02 23:14:22 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
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
#include "document/css/apply.h"
#include "document/css/css.h"
#include "document/css/stylesheet.h"
#include "document/html/frames.h"
#include "document/html/parser/forms.h"
#include "document/html/parser/link.h"
#include "document/html/parser/stack.h"
#include "document/html/parser/parse.h"
#include "document/html/parser.h"
#include "document/html/renderer.h"
#include "document/html/tables.h"
#include "document/renderer.h"
#include "intl/charsets.h"
#include "osdep/ascii.h"
#include "protocol/header.h"
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

unsigned char *
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


struct html_context html_context = {
#ifdef CONFIG_CSS
	INIT_CSS_STYLESHEET(html_context.css_styles, import_css_stylesheet),
#endif
};


void
ln_break(int n, void (*line_break)(void *), void *f)
{
	if (!n || html_top.invisible) return;
	while (n > html_context.line_breax) {
		html_context.line_breax++;
		line_break(f);
	}
	html_context.position = 0;
	html_context.putsp = -1;
}

void
put_chrs(unsigned char *start, int len,
	 void (*put_chars)(void *, unsigned char *, int), void *f)
{
	if (html_is_preformatted())
		html_context.putsp = 0;

	if (!len || html_top.invisible)
		return;

	if (html_context.putsp == 1) {
		put_chars(f, " ", 1);
		html_context.position++;
		html_context.putsp = -1;
	}

	if (html_context.putsp == -1) {
		html_context.putsp = 0;
		if (isspace(start[0])) {
			start++, len--;

			if (!len) {
				if (!html_is_preformatted())
					html_context.putsp = -1;
				return;
			}
		}
	}

	if (isspace(start[len - 1]) && !html_is_preformatted())
		html_context.putsp = -1;
	html_context.was_br = 0;

	put_chars(f, start, len);

	html_context.position += len;
	html_context.line_breax = 0;
	if (html_context.was_li > 0)
		html_context.was_li--;
}

void
set_fragment_identifier(unsigned char *attr_name, unsigned char *attr)
{
	unsigned char *id_attr = get_attr_val(attr_name, attr);

	if (id_attr) {
		html_context.special_f(html_context.part, SP_TAG, id_attr);
		mem_free(id_attr);
	}
}

void
add_fragment_identifier(void *part, unsigned char *attr)
{
	html_context.special_f(part, SP_TAG, attr);
}

#ifdef CONFIG_CSS
void
import_css_stylesheet(struct css_stylesheet *css, unsigned char *url, int len)
{
	unsigned char *import_url;
	struct uri *uri;

	if (!global_doc_opts->css_enable
	    || !global_doc_opts->css_import)
		return;

	url = memacpy(url, len);
	if (!url) return;

	/* HTML <head> urls should already be fine but we can.t detect them. */
	import_url = join_urls(format.href_base, url);
	mem_free(url);

	if (!import_url) return;

	uri = get_uri(import_url, URI_BASE);
	mem_free(import_url);

	if (!uri) return;

	/* Request the imported stylesheet as part of the document ... */
	html_context.special_f(html_context.part, SP_STYLESHEET, uri);

	/* ... and then attempt to import from the cache. */
	import_css(css, uri);

	done_uri(uri);
}
#endif

void
html_span(unsigned char *a)
{
}

void
html_bold(unsigned char *a)
{
	format.attr |= AT_BOLD;
}

void
html_italic(unsigned char *a)
{
	format.attr |= AT_ITALIC;
}

void
html_underline(unsigned char *a)
{
	format.attr |= AT_UNDERLINE;
}

void
html_fixed(unsigned char *a)
{
	format.attr |= AT_FIXED;
}

void
html_subscript(unsigned char *a)
{
	format.attr |= AT_SUBSCRIPT;
}

void
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
void
html_focusable(unsigned char *a)
{
	unsigned char *accesskey;
	int tabindex;

	format.accesskey = 0;
	format.tabindex = 0x80000000;

	if (!a) return;

	accesskey = get_attr_val(a, "accesskey");
	if (accesskey) {
		accesskey[0] = toupper(accesskey[0]);
		format.accesskey = read_key(accesskey);
		mem_free(accesskey);
	}

	tabindex = get_num(a, "tabindex");
	if (tabindex > 0) {
		format.tabindex = (tabindex & 0x7fff) << 16;
	}
}

void
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
	get_color(a, "color", &format.fg);
}

void
html_body(unsigned char *a)
{
	get_color(a, "text", &format.fg);
	get_color(a, "link", &format.clink);
	get_color(a, "vlink", &format.vlink);

	get_bgcolor(a, &format.bg);
#ifdef CONFIG_CSS
	/* If there are any CSS twaks regarding bgcolor, make sure we will get
	 * it _and_ prefer it over bgcolor attribute. */
	if (global_doc_opts->css_enable)
		css_apply(&html_top, &html_context.css_styles);
#endif

	if (par_format.bgcolor != format.bg) {
		/* Modify the root HTML element - format_html_part() will take
		 * this from there. */
		struct html_element *e = html_context.stack.prev;

		e->parattr.bgcolor = e->attr.bg = par_format.bgcolor = format.bg;
	}

	if (html_context.has_link_lines
	    && par_format.bgcolor
	    && !search_html_stack("BODY")) {
		html_context.special_f(html_context.part, SP_COLOR_LINK_LINES);
	}
}

void
html_skip(unsigned char *a)
{
	html_top.invisible = 1;
	html_top.type = ELEMENT_DONT_KILL;
}

void
html_style(unsigned char *a)
{
	html_skip(a);
}

void
html_head(unsigned char *a)
{
	/* This makes sure it gets to the stack and helps tame down unclosed
	 * <title>. */
}

void
html_title(unsigned char *a)
{
	html_top.invisible = 1;
	html_top.type = ELEMENT_WEAK;
}

void
html_center(unsigned char *a)
{
	par_format.align = ALIGN_CENTER;
	if (!html_context.table_level)
		par_format.leftmargin = par_format.rightmargin = 0;
}

void
html_linebrk(unsigned char *a)
{
	unsigned char *al = get_attr_val(a, "align");

	if (al) {
		if (!strcasecmp(al, "left")) par_format.align = ALIGN_LEFT;
		else if (!strcasecmp(al, "right")) par_format.align = ALIGN_RIGHT;
		else if (!strcasecmp(al, "center")) {
			par_format.align = ALIGN_CENTER;
			if (!html_context.table_level)
				par_format.leftmargin = par_format.rightmargin = 0;
		} else if (!strcasecmp(al, "justify")) par_format.align = ALIGN_JUSTIFY;
		mem_free(al);
	}
}

void
html_br(unsigned char *a)
{
	html_linebrk(a);
	if (html_context.was_br)
		ln_break(2, html_context.line_break_f, html_context.part);
	else
		html_context.was_br = 1;
}

void
html_p(unsigned char *a)
{
	int_lower_bound(&par_format.leftmargin, html_context.margin);
	int_lower_bound(&par_format.rightmargin, html_context.margin);
	/*par_format.align = ALIGN_LEFT;*/
	html_linebrk(a);
}

void
html_address(unsigned char *a)
{
	par_format.leftmargin++;
	par_format.align = ALIGN_LEFT;
}

void
html_blockquote(unsigned char *a)
{
	par_format.leftmargin += 2;
	par_format.align = ALIGN_LEFT;
}

void
html_h(int h, unsigned char *a,
       enum format_align default_align)
{
	if (!par_format.align) par_format.align = default_align;
	html_linebrk(a);

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
html_h1(unsigned char *a)
{
	format.attr |= AT_BOLD;
	html_h(1, a, ALIGN_CENTER);
}

void
html_h2(unsigned char *a)
{
	html_h(2, a, ALIGN_LEFT);
}

void
html_h3(unsigned char *a)
{
	html_h(3, a, ALIGN_LEFT);
}

void
html_h4(unsigned char *a)
{
	html_h(4, a, ALIGN_LEFT);
}

void
html_h5(unsigned char *a)
{
	html_h(5, a, ALIGN_LEFT);
}

void
html_h6(unsigned char *a)
{
	html_h(6, a, ALIGN_LEFT);
}

void
html_pre(unsigned char *a)
{
	format.attr |= AT_PREFORMATTED;
	par_format.leftmargin = (par_format.leftmargin > 1);
	par_format.rightmargin = 0;
}

void
html_xmp(unsigned char *a)
{
	html_context.was_xmp = 1;
	html_pre(a);
}

void
html_hr(unsigned char *a)
{
	int i/* = par_format.width - 10*/;
	unsigned char r = (unsigned char)BORDER_DHLINE;
	int q = get_num(a, "size");

	if (q >= 0 && q < 2) r = (unsigned char)BORDER_SHLINE;
	html_stack_dup(ELEMENT_KILLABLE);
	par_format.align = ALIGN_CENTER;
	mem_free_set(&format.link, NULL);
	format.form = NULL;
	html_linebrk(a);
	if (par_format.align == ALIGN_JUSTIFY) par_format.align = ALIGN_CENTER;
	par_format.leftmargin = par_format.rightmargin = html_context.margin;

	i = get_width(a, "width", 1);
	if (i == -1) i = par_format.width - (html_context.margin - 2) * 2;
	format.attr = AT_GRAPHICS;
	html_context.special_f(html_context.part, SP_NOWRAP, 1);
	while (i-- > 0) {
		put_chrs(&r, 1, html_context.put_chars_f, html_context.part);
	}
	html_context.special_f(html_context.part, SP_NOWRAP, 0);
	ln_break(2, html_context.line_break_f, html_context.part);
	kill_html_stack_item(&html_top);
}

void
html_table(unsigned char *a)
{
	par_format.leftmargin = par_format.rightmargin = html_context.margin;
	par_format.align = ALIGN_LEFT;
	html_linebrk(a);
	format.attr = 0;
}

void
html_tr(unsigned char *a)
{
	html_linebrk(a);
}

void
html_th(unsigned char *a)
{
	/*html_linebrk(a);*/
	kill_html_stack_until(1, "TD", "TH", "", "TR", "TABLE", NULL);
	format.attr |= AT_BOLD;
	put_chrs(" ", 1, html_context.put_chars_f, html_context.part);
}

void
html_td(unsigned char *a)
{
	/*html_linebrk(a);*/
	kill_html_stack_until(1, "TD", "TH", "", "TR", "TABLE", NULL);
	format.attr &= ~AT_BOLD;
	put_chrs(" ", 1, html_context.put_chars_f, html_context.part);
}

void
html_base(unsigned char *a)
{
	unsigned char *al = get_url_val(a, "href");

	if (al) {
		struct html_element *element = html_context.stack.prev;
		unsigned char *base = join_urls(element->attr.href_base, al);
		struct uri *uri = base ? get_uri(base, 0) : NULL;

		mem_free(al);
		mem_free_if(base);

		if (!uri) return;

		/* Now distribute the base URL */
		foreach (element, html_context.stack) {
			if (compare_uri(element->attr.href_base, uri, 0))
				continue;

			done_uri(element->attr.href_base);
			element->attr.href_base = get_uri_reference(uri);
		}

		done_uri(uri);
	}

	al = get_target(a);
	if (al) mem_free_set(&format.target_base, al);
}

void
html_ul(unsigned char *a)
{
	unsigned char *al;

	/* dump_html_stack(); */
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
	if (!html_context.table_level)
		int_upper_bound(&par_format.leftmargin, par_format.width / 2);

	par_format.align = ALIGN_LEFT;
	html_top.type = ELEMENT_DONT_KILL;
}

void
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
	if (!html_context.table_level)
		int_upper_bound(&par_format.leftmargin, par_format.width / 2);

	par_format.align = ALIGN_LEFT;
	html_top.type = ELEMENT_DONT_KILL;
}

void
html_li(unsigned char *a)
{
	/* When handling the code <li><li> @was_li will be 1 and it means we
	 * have to insert a line break since no list item content has done it
	 * for us. */
	if (html_context.was_li) {
		html_context.line_breax = 0;
		ln_break(1, html_context.line_break_f, html_context.part);
	}

	/*kill_html_stack_until(0, "", "UL", "OL", NULL);*/
	if (!par_format.list_number) {
		unsigned char x[7] = "*&nbsp;";
		int t = par_format.flags & P_LISTMASK;

		if (t == P_O) x[0] = 'o';
		if (t == P_PLUS) x[0] = '+';
		put_chrs(x, 7, html_context.put_chars_f, html_context.part);
		par_format.leftmargin += 2;
		par_format.align = ALIGN_LEFT;

	} else {
		unsigned char c = 0;
		unsigned char n[32];
		int nlen;
		int t = par_format.flags & P_LISTMASK;
		int s = get_num(a, "value");

		if (s != -1) par_format.list_number = s;

		if (t == P_ALPHA || t == P_alpha) {
			put_chrs("&nbsp;", 6, html_context.put_chars_f, html_context.part);
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
				put_chrs("&nbsp;", 6, html_context.put_chars_f, html_context.part);
				c = 1;
			}

			ulongcat(n, NULL, par_format.list_number, (sizeof(n) - 1), 0);
		}

		nlen = strlen(n);
		put_chrs(n, nlen, html_context.put_chars_f, html_context.part);
		put_chrs(".&nbsp;", 7, html_context.put_chars_f, html_context.part);
		par_format.leftmargin += nlen + c + 2;
		par_format.align = ALIGN_LEFT;
		html_top.next->parattr.list_number = par_format.list_number + 1;
		par_format.list_number = 0;
	}

	html_context.putsp = -1;
	html_context.line_breax = 2;
	html_context.was_li = 1;
}

void
html_dl(unsigned char *a)
{
	par_format.flags &= ~P_COMPACT;
	if (has_attr(a, "compact")) par_format.flags |= P_COMPACT;
	if (par_format.list_level) par_format.leftmargin += 5;
	par_format.list_level++;
	par_format.list_number = 0;
	par_format.align = ALIGN_LEFT;
	par_format.dd_margin = par_format.leftmargin;
	html_top.type = ELEMENT_DONT_KILL;
	if (!(par_format.flags & P_COMPACT)) {
		ln_break(2, html_context.line_break_f, html_context.part);
		html_top.linebreak = 2;
	}
}

void
html_dt(unsigned char *a)
{
	kill_html_stack_until(0, "", "DL", NULL);
	par_format.align = ALIGN_LEFT;
	par_format.leftmargin = par_format.dd_margin;
	if (!(par_format.flags & P_COMPACT) && !has_attr(a, "compact"))
		ln_break(2, html_context.line_break_f, html_context.part);
}

void
html_dd(unsigned char *a)
{
	kill_html_stack_until(0, "", "DL", NULL);

	par_format.leftmargin = par_format.dd_margin
				+ (html_context.table_level ? 3 : 8);
	if (!html_context.table_level)
		int_upper_bound(&par_format.leftmargin, par_format.width / 2);
	par_format.align = ALIGN_LEFT;
}



void
html_noframes(unsigned char *a)
{
	struct html_element *element;

	if (!global_doc_opts->frames) return;

	element = search_html_stack("frameset");
	if (element && !element->frameset) return;

	html_skip(a);
}

void
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
		if (html_context.special_f(html_context.part, SP_USED, NULL)) {
			html_context.special_f(html_context.part, SP_FRAME,
					       html_top.frameset, name, url);
		}
	}

	mem_free(name);
	mem_free(url);
}

void
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
	    || !global_doc_opts->frames
	    || !html_context.special_f(html_context.part, SP_USED, NULL))
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
		width = global_doc_opts->box.width;
		height = global_doc_opts->box.height;
		global_doc_opts->needs_height = 1;
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
		html_top.frameset = html_context.special_f(html_context.part, SP_FRAMESET, &fp);
	}
	mem_free_if(fp.width);
	mem_free_if(fp.height);

free_and_return:
	mem_free(cols);
	mem_free(rows);
}





void
process_head(unsigned char *head)
{
	unsigned char *refresh, *url;

	refresh = parse_header(head, "Refresh", NULL);
	if (refresh) {
		url = parse_header_param(refresh, "URL");
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
			html_context.special_f(html_context.part, SP_REFRESH, seconds, url);
			mem_free(url);
			mem_free(saved_url);
		}
		mem_free(refresh);
	}
}




static int
look_for_map(unsigned char **pos, unsigned char *eof, struct uri *uri)
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

	if (uri && uri->fragment) {
		al = get_attr_val(attr, "name");
		if (!al) return 1;

		if (strlcasecmp(al, -1, uri->fragment, uri->fragmentlen)) {
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
look_for_link(unsigned char **pos, unsigned char *eof, struct menu_item **menu,
	      struct memory_list **ml, struct uri *href_base,
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
			label = convert_string(ct, alt, strlen(alt), CSM_DEFAULT, NULL);
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


int
get_image_map(unsigned char *head, unsigned char *pos, unsigned char *eof,
	      struct menu_item **menu, struct memory_list **ml, struct uri *uri,
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

	while (look_for_map(&pos, eof, uri));

	if (pos >= eof) {
		mem_free(*menu);
		return -1;
	}

	*ml = NULL;

	while (look_for_link(&pos, eof, menu, ml, uri, target_base, ct)) ;

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
	html_context.line_breax = 1;

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
init_html_parser(struct uri *uri, struct document_options *options,
		 unsigned char *start, unsigned char *end,
		 struct string *head, struct string *title,
		 void (*put_chars)(void *, unsigned char *, int),
		 void (*line_break)(void *),
		 void *(*special)(void *, enum html_special_type, ...))
{
	struct html_element *e;

	assert(uri && options);
	if_assert_failed return;

	init_list(html_context.stack);

	html_context.startf = start;
	html_context.put_chars_f = put_chars;
	html_context.line_break_f = line_break;
	html_context.special_f = special;
	scan_http_equiv(start, end, head, title);

	e = mem_calloc(1, sizeof(struct html_element));
	if (!e) return;

	add_to_list(html_context.stack, e);

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

	format.href_base = get_uri_reference(uri);
	format.target_base = null_or_stracpy(options->framename);

	par_format.align = ALIGN_LEFT;
	par_format.leftmargin = options->margin;
	par_format.rightmargin = options->margin;

	par_format.width = options->box.width;
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

	html_context.has_link_lines = 0;
	html_context.table_level = 0;
	html_context.last_form_tag = NULL;
	html_context.last_form_attr = NULL;
	html_context.last_input_tag = NULL;

#ifdef CONFIG_CSS
	mirror_css_stylesheet(&html_context.css_styles, &default_stylesheet);
#endif
}

void
done_html_parser(void)
{
#ifdef CONFIG_CSS
	if (global_doc_opts->css_enable)
		done_css_stylesheet(&html_context.css_styles);
#endif

	done_form();

	kill_html_stack_item(html_context.stack.next);

	assertm(list_empty(html_context.stack),
		"html stack not empty after operation");
	if_assert_failed init_list(html_context.stack);
}
