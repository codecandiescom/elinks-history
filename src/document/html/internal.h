/* $Id: internal.h,v 1.6 2004/04/24 00:57:24 pasky Exp $ */

#ifndef EL__DOCUMENT_HTML_INTERNAL_H
#define EL__DOCUMENT_HTML_INTERNAL_H

#include "document/css/stylesheet.h"
#include "document/html/parser.h"
#include "util/lists.h"

extern struct list_head html_stack;

#define format (((struct html_element *) html_stack.next)->attr)
#define par_format (((struct html_element *) html_stack.next)->parattr)
#define html_top (*(struct html_element *) html_stack.next)

extern void *ff;
extern void (*put_chars_f)(void *, unsigned char *, int);
extern void (*line_break_f)(void *);
extern void *(*special_f)(void *, enum html_special_type, ...);

void ln_break(int n, void (*line_break)(void *), void *f);

/* For parser/parse.c: */

int do_html_select(unsigned char *attr, unsigned char *html, unsigned char *eof, unsigned char **end, void *f);
void do_html_textarea(unsigned char *attr, unsigned char *html, unsigned char *eof, unsigned char **end, void *f);
void process_head(unsigned char *head);
void put_chrs(unsigned char *start, int len, void (*put_chars)(void *, unsigned char *, int), void *f);

extern unsigned char *eoff;
extern int line_breax;
extern int position;
extern int putsp;
extern int was_br;
extern int was_li;
extern int was_xmp;
extern struct css_stylesheet css_styles;

/* For parser/link.c: */

extern int has_link_lines;

void html_focusable(unsigned char *a);
void html_skip(unsigned char *a);
unsigned char *get_target(unsigned char *a);
void import_css_stylesheet(struct css_stylesheet *css, unsigned char *url, int len);

#endif
