/* $Id: forms.h,v 1.2 2004/04/29 13:30:00 zas Exp $ */

#ifndef EL__DOCUMENT_HTML_PARSER_FORMS_H
#define EL__DOCUMENT_HTML_PARSER_FORMS_H

void done_form(void);

void html_button(unsigned char *a);
void html_form(unsigned char *a);
void html_input(unsigned char *a);
void html_select(unsigned char *a);
void html_option(unsigned char *a);
void html_textarea(unsigned char *a);

int do_html_select(unsigned char *attr, unsigned char *html, unsigned char *eof, unsigned char **end, void *f);
void do_html_textarea(unsigned char *attr, unsigned char *html, unsigned char *eof, unsigned char **end, void *f);

#endif
