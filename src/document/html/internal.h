/* $Id: internal.h,v 1.29 2004/06/23 10:53:54 jonas Exp $ */

#ifndef EL__DOCUMENT_HTML_INTERNAL_H
#define EL__DOCUMENT_HTML_INTERNAL_H

#include "document/css/stylesheet.h"
#include "document/html/parser.h"
#include "util/lists.h"

void ln_break(int n, void (*line_break)(void *), void *f);

/* For parser/parse.c: */

void process_head(unsigned char *head);
void put_chrs(unsigned char *start, int len, void (*put_chars)(void *, unsigned char *, int), void *f);

struct html_context {
#ifdef CONFIG_CSS
	/* The default stylesheet is initially merged into it. When parsing CSS
	 * from <style>-tags and external stylesheets if enabled is merged
	 * added to it. */
	struct css_stylesheet css_styles;
#endif

	/* For:
	 * html/parser/parse.c
	 * html/parser/stack.c
	 * html/parser.c */
	struct list_head stack;

	/* For parser/parse.c: */
	unsigned char *eoff; /* For parser/forms.c too */
	int line_breax;
	int position;
	int putsp;
	int was_br; /* For parser/forms.c too */
	int was_li;
	int was_xmp;

	/* For html/parser.c, html/renderer.c */
	int margin;

	/* For parser/link.c: */
	int has_link_lines;

	/* For parser/forms.c: */
	unsigned char *eofff;
	unsigned char *startf;
	unsigned char *last_form_tag;
	unsigned char *last_form_attr;
	unsigned char *last_input_tag;

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
	void *ff;

	/* For:
	 * html/parser/forms.c
	 * html/parser/link.c
	 * html/parser/parse.c
	 * html/parser.c */
	void (*put_chars_f)(void *, unsigned char *, int);

	/* For:
	 * html/parser/forms.c
	 * html/parser/link.c
	 * html/parser/parse.c
	 * html/parser/stack.c
	 * html/parser.c */
	void (*line_break_f)(void *);

	/* For:
	 * html/parser/forms.c
	 * html/parser/parse.c
	 * html/parser.c */
	void *(*special_f)(void *, enum html_special_type, ...);
};

#define format (((struct html_element *) html_context.stack.next)->attr)
#define par_format (((struct html_element *) html_context.stack.next)->parattr)
#define html_top (*(struct html_element *) html_context.stack.next)


extern struct html_context html_context;

/* For parser/link.c: */

void html_focusable(unsigned char *a);
void html_skip(unsigned char *a);
unsigned char *get_target(unsigned char *a);
void import_css_stylesheet(struct css_stylesheet *css, unsigned char *url, int len);

#endif
