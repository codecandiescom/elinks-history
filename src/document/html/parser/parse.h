/* $Id: parse.h,v 1.4 2004/05/07 08:42:47 zas Exp $ */

#ifndef EL__DOCUMENT_HTML_PARSER_PARSE_H
#define EL__DOCUMENT_HTML_PARSER_PARSE_H

struct string;

/* Flags for get_attr_val_(). */
enum html_attr_flags {
	HTML_ATTR_NONE = 0,

	/* If HTML_ATTR_TEST is set then we only test for existence of
	 * an attribute of that @name. In that mode it returns NULL if
	 * attribute was not found, and a pointer to start of the attribute
	 * if it was found. */
	HTML_ATTR_TEST = 1,

	/* If HTML_ATTR_EAT_NL is not set, newline and tabs chars are
	 * replaced by spaces in returned value, else these chars are
	 * skipped. */
	HTML_ATTR_EAT_NL = 2,

	/* If HTML_ATTR_NO_CONV is set, then convert_string() is not called
	 * on value. */
	HTML_ATTR_NO_CONV = 4,
};

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
