/* $Id: scanner.h,v 1.3 2003/02/25 14:15:50 jonas Exp $ */

#ifndef EL__USIVE_PARSER_CSS_SCANNER_H
#define EL__USIVE_PARSER_CSS_SCANNER_H

#include "elusive/parser/css/state.h"

/* The scanner table */
/* Contains bitmaps for the various CSS characters groups.
 * Idea sync'ed from mozilla browser. */
extern int css_scan_table[];

/* Bitmap entries for the CSS character groups used in the scanner table */

#define	IS_ALPHA	(1 << 0)
#define	IS_DIGIT	(1 << 1)
#define	IS_HEX_DIGIT	(1 << 2)
#define	IS_IDENT_START	(1 << 3)
#define	IS_IDENT	(1 << 4)
#define	IS_WHITESPACE	(1 << 5)
#define	IS_NEWLINE	(1 << 6)
#define	IS_NON_ASCII	(1 << 7)

/* Initialize the scanner table */
void css_init_scan_table(void);


/* Token scanners */
/* Generates tokens and passes them by modifying the given pointer to pointers
 * in the parser state. Failures will be signaled by setting
 *
 *	css_parser_state->data.token.len = -1
 *
 * In this case the src and len parameters will then not changed */

/* Scanning of identifiers */
enum pstate_code
css_scan_ident(struct parser_state *, unsigned char **src, int *len);

/* Scanning of names */
enum pstate_code
css_scan_name(struct parser_state *state, unsigned char **src, int *len);

/* String scanning */
/* The generated token will not include the string delimiters. */
enum pstate_code
css_scan_string(struct parser_state *state, unsigned char **src, int *len);

/* Scanning of url functions */
/* The generated token will only contain the url. */
enum pstate_code
css_scan_url(struct parser_state *state, unsigned char **src, int *len);

/* Scanning of escape sequences */
enum pstate_code
css_scan_escape(struct parser_state *state, unsigned char **src, int *len);

/* Scanning of unicoderanges */
enum pstate_code
css_scan_unicoderange(struct parser_state *state, unsigned char **src, int *len);

/* Scanning of hexcolors */
enum pstate_code
css_scan_hexcolor(struct parser_state *state, unsigned char **src, int *len);


/* Code skipping scanners */
/* Primary usage is for recovering or ignoring code snippets. */

/* Scanning of comments */
/* For scanning comments in loops the macro below should be used. */
enum pstate_code
css_scan_comment(struct parser_state *state, unsigned char **src, int *len);

/* Scanner for skipping until a given character */
enum pstate_code
css_scan_skip_until(struct parser_state *, unsigned char **, int *len);

/* Scanner for skipping a block or up till next ';'. What ever comes first. */
enum pstate_code
css_scan_skip(struct parser_state *, unsigned char **, int *len);

/* Macros for skipping tokens. XXX: Engineered for loops. */

#define CSS_SKIP_WHITESPACE(source, length) \
	if (css_scan_table[*(source)] & IS_WHITESPACE) { \
		do { \
			(source)++; (length)--; \
		} while (length && (css_scan_table[*(source)] & IS_WHITESPACE)); \
		continue; \
	}

#define CSS_SKIP_COMMENT(parserstate, sourceptr, lengthptr, source, length) \
	if (*source == '/') { \
		if (length < 2) return PSTATE_SUSPEND; \
		if (*(source+1) == '*') { \
			*sourceptr = source, *lengthptr = length; \
			css_state_push(parserstate, CSS_COMMENT); \
			return PSTATE_CHANGE; \
		} \
	}

#endif
