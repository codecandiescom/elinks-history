/* $Id: util.c,v 1.1 2003/02/25 14:15:50 jonas Exp $ */

#ifndef EL__USIVE_PARSER_CSS_UTIL_H
#define EL__USIVE_PARSER_CSS_UTIL_H

#include "elusive/parser/css/util.h"

enum mediatype
string2mediatype(unsigned char *name, int name_len);

unsigned char *
mediatype2string(enum mediatype mediatype);

#endif /* EL__USIVE_PARSER_CSS_UTIL_H */
