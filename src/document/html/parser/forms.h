/* $Id: forms.h,v 1.7 2005/07/09 01:30:08 miciah Exp $ */

#ifndef EL__DOCUMENT_HTML_PARSER_FORMS_H
#define EL__DOCUMENT_HTML_PARSER_FORMS_H

struct html_context;

void html_button(unsigned char *a);
void html_form(unsigned char *a);
void html_input(unsigned char *a);
void html_select(unsigned char *a);
void html_option(unsigned char *a);
void html_textarea(unsigned char *a);

int do_html_select(unsigned char *attr, unsigned char *html, unsigned char *eof, unsigned char **end);
void do_html_textarea(unsigned char *attr, unsigned char *html, unsigned char *eof, unsigned char **end, struct html_context *html_context);

#endif
