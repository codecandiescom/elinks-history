/* $Id: charsets.h,v 1.7 2003/05/28 00:58:41 zas Exp $ */

#ifndef EL__CHARSETS_H
#define EL__CHARSETS_H

#include "intl/codepage.h"
#include "util/types.h"

typedef uint32_t unicode_val;

/* UCS/Unicode replacement character */
#define UCS_NO_CHAR ((unicode_val) 0xFFFD)

struct conv_table {
	int t;
	union {
		unsigned char *str;
		struct conv_table *tbl;
	} u;
};

struct conv_table *get_translation_table(int, int);
unsigned char *get_entity_string(const unsigned char *, const int, const int);
unsigned char *convert_string(struct conv_table *, unsigned char *, int);
int get_cp_index(unsigned char *);
unsigned char *get_cp_name(int);
unsigned char *get_cp_mime_name(int);
int is_cp_special(int);
void free_conv_table(void);
unsigned char *cp2utf_8(int, int);
unsigned char *u2cp(unicode_val, int);

#endif
