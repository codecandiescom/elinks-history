/* $Id: internal.h,v 1.49 2005/07/09 19:31:40 miciah Exp $ */

#ifndef EL__DOCUMENT_HTML_INTERNAL_H
#define EL__DOCUMENT_HTML_INTERNAL_H

#include "document/css/stylesheet.h"
#include "document/html/parser.h"
#include "util/lists.h"

struct uri;

/* For parser/parse.c: */

void process_head(unsigned char *head, struct html_context *html_context);
void put_chrs(unsigned char *start, int len, struct html_context *html_context);

enum html_whitespace_state {
	/* Either we are starting a new "block" or the last segment of the
	 * current "block" is ending with whitespace and we should eat any
	 * leading whitespace of the next segment passed to put_chrs().
	 * This prevents HTML whitespace from indenting new blocks by one
	 * or creating two consecutive segments of whitespace in the middle
	 * of a block. */
	HTML_SPACE_SUPPRESS,

	/* Do not do anything special.  */
	HTML_SPACE_NORMAL,

	/* We should add a space when we start the next segment if it doesn't
	 * already start with whitespace. This is used in an "x  </y>  z"
	 * scenario when the parser hits </y>: it renders "x" and sets this,
	 * so that it will then render " z". XXX: Then we could of course
	 * render "x " and set -1. But we test for this value in parse_html()
	 * if we hit an opening tag of an element and potentially
	 * put_chrs(" "). That needs more investigation yet. --pasky */
	HTML_SPACE_ADD,
};

struct html_context {
#ifdef CONFIG_CSS
	/* The default stylesheet is initially merged into it. When parsing CSS
	 * from <style>-tags and external stylesheets if enabled is merged
	 * added to it. */
	struct css_stylesheet css_styles;
#endif

	/* These are global per-document base values, alterable by the <base>
	 * element. */
	struct uri *base_href;
	unsigned char *base_target;

	/* For:
	 * html/parser/parse.c
	 * html/parser/stack.c
	 * html/parser.c */
	struct list_head stack;

	/* For parser/parse.c: */
	unsigned char *eoff; /* For parser/forms.c too */
	int line_breax; /* This is for ln_break. */
	int position;
	enum html_whitespace_state putsp; /* This is for the put_chrs
					   * state-machine. */
	int was_br; /* For parser/forms.c too */
	int was_li;
	int was_xmp;

	/* For html/parser.c, html/renderer.c */
	int margin;

	/* For parser/link.c: */
	int has_link_lines;

	/* For parser/forms.c: */
	unsigned char *startf;

	/* For:
	 * html/parser/parse.c
	 * html/parser.c
	 * html/renderer.c
	 * html/tables.c */
	int table_level;

	/* For:
	 * html/parser/forms.c
	 * html/parser/link.c
	 * html/parser/parse.c
	 * html/parser/stack.c
	 * html/parser.c */
	struct part *part;

	/* For:
	 * html/parser/forms.c
	 * html/parser/link.c
	 * html/parser/parse.c
	 * html/parser.c */
	void (*put_chars_f)(struct part *, unsigned char *, int);

	/* For:
	 * html/parser/forms.c
	 * html/parser/link.c
	 * html/parser/parse.c
	 * html/parser/stack.c
	 * html/parser.c */
	void (*line_break_f)(struct part *);

	/* For:
	 * html/parser/forms.c
	 * html/parser/parse.c
	 * html/parser.c */
	void *(*special_f)(struct part *, enum html_special_type, ...);
};

#define format (((struct html_element *) global_html_context.stack.next)->attr)
#define par_format (((struct html_element *) global_html_context.stack.next)->parattr)
#define html_top (*(struct html_element *) global_html_context.stack.next)

#define html_is_preformatted() (format.style.attr & AT_PREFORMATTED)

#define get_html_max_width() \
	int_max(par_format.width - (par_format.leftmargin + par_format.rightmargin), 0)

extern struct html_context global_html_context;

/* For parser/link.c: */

void html_focusable(unsigned char *a, struct html_context *html_context);
void html_skip(unsigned char *a, struct html_context *html_context);
unsigned char *get_target(unsigned char *a);

void
import_css_stylesheet(struct css_stylesheet *css, struct uri *base_uri,
		      unsigned char *url, int len);

#endif
