/* $Id: util.h,v 1.1 2003/02/25 14:15:50 jonas Exp $ */

#ifndef EL__USIVE_PARSER_CSS_UTIL_H
#define EL__USIVE_PARSER_CSS_UTIL_H

/* This is used as a string container of selectors or attribute names and
 * values etc. */
struct css_string {
	/* This member will always point to the string
	 * value that should be used - if nothing has to be escaped this will
	 * point directly into the document source. */

	unsigned char *str;
	int strlen;

	/* This is a pointer to the source string containing the place of
	 * occurence of the element name or attribute value. This is only
	 * needed when str doesn't point there mainly if an escape sequence was
	 * used in the identifier. Thus, if src is non-NULL, str will be
	 * mem_free()'d, otherwise it won't. */
	unsigned char *src;
};

/* Mediatype bitmap entries used for setting the mediatypes to be accepted */

enum mediatype {
	MEDIATYPE_ALL		= (1 <<  0), /* All devices */
	MEDIATYPE_AURAL	 	= (1 <<  1), /* Speech synthesizers */
	MEDIATYPE_BRAILLE	= (1 <<  2), /* Braille tactile feedback devs */
	MEDIATYPE_EMBOSSED	= (1 <<  3), /* Paged braille printers */
	MEDIATYPE_HANDHELD	= (1 <<  4), /* Handheld devices */
	MEDIATYPE_PRINT		= (1 <<  5), /* Paged, opaque material ... */
	MEDIATYPE_PROJECTION	= (1 <<  6), /* Projected presentations */
	MEDIATYPE_SCREEN	= (1 <<  7), /* Color computer screens */
	MEDIATYPE_TTY		= (1 <<  8), /* Teletypes, terminals ... */
	MEDIATYPE_TV		= (1 <<  9), /* Television-type devices */
};

enum mediatype
string2mediatype(unsigned char *name, int name_len);

unsigned char *
mediatype2string(enum mediatype mediatype);

#endif /* EL__USIVE_PARSER_CSS_UTIL_H */
