/* $Id: parse.h,v 1.1 2004/04/24 00:33:14 pasky Exp $ */

#ifndef EL__DOCUMENT_HTML_PARSER_PARSE_H
#define EL__DOCUMENT_HTML_PARSER_PARSE_H

struct string;


/* Interface for both the renderer and the table handling */

void parse_html(unsigned char *html, unsigned char *eof, void *f, unsigned char *head);


/* Interface for the table handling */

int parse_element(unsigned char *, unsigned char *, unsigned char **, int *, unsigned char **, unsigned char **);

unsigned char *get_attr_val(unsigned char *e, unsigned char *name);
unsigned char *get_url_val(unsigned char *e, unsigned char *name);
int has_attr(unsigned char *, unsigned char *);
int get_num(unsigned char *, unsigned char *);
int get_width(unsigned char *, unsigned char *, int);

unsigned char *skip_comment(unsigned char *, unsigned char *);


void scan_http_equiv(unsigned char *s, unsigned char *eof, struct string *head, struct string *title);


/* Lifecycle functions for the tags fastfind cache, if being in use. */

void free_tags_lookup(void);
void init_tags_lookup(void);

#endif
