/* $Id: parse.h,v 1.2 2004/05/06 23:39:37 zas Exp $ */

#ifndef EL__DOCUMENT_HTML_PARSER_PARSE_H
#define EL__DOCUMENT_HTML_PARSER_PARSE_H

struct string;

/* Flags for get_attr_value(). */
enum gav_flags {
	GAV_NONE = 0,
	GAV_TEST = 1,   /* Test only for attribute presence. */
	GAV_EAT_NL = 2, /* Eat newlines in attribute value. */
	GAV_NO_CONV = 4,/* Don't call convert_string on value. Not yet used. */
};

/* Parses html element attributes. */
/* - e is attr pointer previously get from parse_element,
 * DON'T PASS HERE ANY OTHER VALUE!!!
 * - name is searched attribute */
/* Returns allocated string containing the attribute, or NULL on unsuccess.
 * If @test_only is different from zero then we only test for existence of
 * an attribute of that @name. In that mode it returns NULL if attribute
 * was not found, and a pointer to start of the attribute if it was found.
 * If @eat_nl is zero, newline and tabs chars are replaced by spaces
 * in returned value, else these chars are skipped. */
unsigned char *get_attr_value(register unsigned char *e, unsigned char *name, enum gav_flags flags);

#define get_attr_val(e, name) get_attr_value(e, name, GAV_NONE)
#define get_url_val(e, name) get_attr_value(e, name, GAV_EAT_NL)
#define has_attr(e, name) (!!get_attr_value(e, name, GAV_TEST))

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
