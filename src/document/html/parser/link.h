/* $Id: link.h,v 1.3 2005/07/09 01:46:49 miciah Exp $ */

#ifndef EL__DOCUMENT_HTML_PARSER_LINK_H
#define EL__DOCUMENT_HTML_PARSER_LINK_H

struct html_context;

void put_link_line(unsigned char *prefix, unsigned char *linkname, unsigned char *link, unsigned char *target, struct html_context *html_context);

void html_a(unsigned char *a);
void html_applet(unsigned char *a);
void html_iframe(unsigned char *a);
void html_img(unsigned char *a);
void html_link(unsigned char *a);
void html_object(unsigned char *a);
void html_embed(unsigned char *a);

#endif
