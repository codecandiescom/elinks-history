/* $Id: atrule.h,v 1.1 2003/01/19 20:25:21 jonas Exp $ */

#ifndef EL__USIVE_PARSER_CSS_ATRULE_H
#define EL__USIVE_PARSER_CSS_ATRULE_H

#include "elusive/parser/parser.h"

/* Mediatype bitmap entries used for setting the mediatypes to be accepted */

#define MEDIATYPE_ALL		(1 <<  0) /* All devices */
#define MEDIATYPE_AURAL	 	(1 <<  1) /* Speech synthesizers */
#define MEDIATYPE_BRAILLE	(1 <<  2) /* Braille tactile feedback devices */
#define MEDIATYPE_EMBOSSED	(1 <<  3) /* Paged braille printers */
#define MEDIATYPE_HANDHELD	(1 <<  4) /* Handheld devices */
#define MEDIATYPE_PRINT		(1 <<  5) /* Paged, opaque material ... */
#define MEDIATYPE_PROJECTION	(1 <<  6) /* Projected presentations */
#define MEDIATYPE_SCREEN	(1 <<  7) /* Color computer screens */
#define MEDIATYPE_TTY		(1 <<  8) /* Teletypes, terminals ... */
#define MEDIATYPE_TV		(1 <<  9) /* Television-type devices */

/* General atrule parser multiplexor */
/* Specialized atrule parsers can only be interfaced through this one since
 * they assume '@<atrule name>' has been skipped. The atrule state will be
 * replaced by a specialized atrule state. */
enum pstate_code
parse_atrule(struct parser_state *state, unsigned char **src, int *len);

/* Parsing of @charset rules */
enum pstate_code
parse_charset(struct parser_state *state, unsigned char **src, int *len);

/* Parsing of @import rules */
enum pstate_code
parse_import(struct parser_state *state, unsigned char **src, int *len);

/* Parsing of @media rules */
enum pstate_code
parse_media(struct parser_state *state, unsigned char **src, int *len);

/* Mediatypes parsing */
/* This is used by @import and @media so placed here */
enum pstate_code
parse_mediatypes(struct parser_state *state, unsigned char **src, int *len);

/* Parsing of @page rules */
/* Is currently just be skipped and ignored */
enum pstate_code
parse_page(struct parser_state *state, unsigned char **src, int *len);

/* Parsin of @font-face rules */
/* Is currently just be skipped and ignored */
enum pstate_code
parse_fontface(struct parser_state *state, unsigned char **src, int *len);

#endif /* EL__USIVE_PARSER_CSS_ATRULE_H */
