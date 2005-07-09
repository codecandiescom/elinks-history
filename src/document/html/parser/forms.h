/* $Id: forms.h,v 1.6 2005/07/09 01:26:50 miciah Exp $ */

#ifndef EL__DOCUMENT_HTML_PARSER_FORMS_H
#define EL__DOCUMENT_HTML_PARSER_FORMS_H

struct html_context;
struct part;

void html_button(unsigned char *a);
void html_form(unsigned char *a);
void html_input(unsigned char *a);
void html_select(unsigned char *a);
void html_option(unsigned char *a);
void html_textarea(unsigned char *a);

int do_html_select(unsigned char *attr, unsigned char *html, unsigned char *eof, unsigned char **end, struct part *part);
void do_html_textarea(unsigned char *attr, unsigned char *html, unsigned char *eof, unsigned char **end, struct html_context *html_context);

#endif
