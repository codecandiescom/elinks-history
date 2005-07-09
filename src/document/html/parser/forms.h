/* $Id: forms.h,v 1.5 2005/07/09 01:23:18 miciah Exp $ */

#ifndef EL__DOCUMENT_HTML_PARSER_FORMS_H
#define EL__DOCUMENT_HTML_PARSER_FORMS_H

struct part;

void html_button(unsigned char *a);
void html_form(unsigned char *a);
void html_input(unsigned char *a);
void html_select(unsigned char *a);
void html_option(unsigned char *a);
void html_textarea(unsigned char *a);

int do_html_select(unsigned char *attr, unsigned char *html, unsigned char *eof, unsigned char **end, struct part *part);
void do_html_textarea(unsigned char *attr, unsigned char *html, unsigned char *eof, unsigned char **end);

#endif
