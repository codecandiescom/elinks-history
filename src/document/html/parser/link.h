/* $Id: link.h,v 1.1 2004/04/24 00:57:24 pasky Exp $ */

#ifndef EL__DOCUMENT_HTML_PARSER_LINK_H
#define EL__DOCUMENT_HTML_PARSER_LINK_H

void put_link_line(unsigned char *prefix, unsigned char *linkname, unsigned char *link, unsigned char *target);

void html_a(unsigned char *a);
void html_applet(unsigned char *a);
void html_iframe(unsigned char *a);
void html_img(unsigned char *a);
void html_link(unsigned char *a);
void html_object(unsigned char *a);

#endif
