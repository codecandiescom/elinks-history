/* $Id: atrule.h,v 1.3 2003/02/25 14:15:50 jonas Exp $ */

#ifndef EL__USIVE_PARSER_CSS_ATRULE_H
#define EL__USIVE_PARSER_CSS_ATRULE_H

#include "elusive/parser/parser.h"

/* General atrule parser multiplexor */
/* Specialized atrule parsers can only be interfaced through this one since
 * they assume '@<atrule name>' has been skipped. The atrule state will be
 * replaced by a specialized atrule state. */
enum pstate_code
css_parse_atrule(struct parser_state *state, unsigned char **src, int *len);

/* Parsing of @charset rules */
enum pstate_code
css_parse_charset(struct parser_state *state, unsigned char **src, int *len);

/* Parsing of @import rules */
enum pstate_code
css_parse_import(struct parser_state *state, unsigned char **src, int *len);

/* Parsing of @media rules */
enum pstate_code
css_parse_media(struct parser_state *state, unsigned char **src, int *len);

/* Mediatypes parsing */
/* This is used by @import and @media so placed here */
enum pstate_code
css_parse_mediatypes(struct parser_state *state, unsigned char **src, int *len);

/* Parsing of @page rules */
/* Is currently just be skipped and ignored */
enum pstate_code
css_parse_page(struct parser_state *state, unsigned char **src, int *len);

/* Parsin of @font-face rules */
/* Is currently just be skipped and ignored */
enum pstate_code
css_parse_fontface(struct parser_state *state, unsigned char **src, int *len);

#endif /* EL__USIVE_PARSER_CSS_ATRULE_H */
