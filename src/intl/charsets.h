/* $Id: charsets.h,v 1.2 2002/04/26 17:26:47 pasky Exp $ */

#ifndef EL__CHARSETS_H
#define EL__CHARSETS_H

/* UCS/Unicode replacement character */
#define UCS_NO_CHAR 0xFFFD

#include <intl/codepage.h>

struct conv_table {
	int t;
	union {
		unsigned char *str;
		struct conv_table *tbl;
	} u;
};

struct conv_table *get_translation_table(int, int);
unsigned char *get_entity_string(unsigned char *, int, int);
unsigned char *convert_string(struct conv_table *, unsigned char *, int);
int get_cp_index(unsigned char *);
unsigned char *get_cp_name(int);
unsigned char *get_cp_mime_name(int);
int is_cp_special(int);
void free_conv_table();
unsigned char *cp2utf_8(int, int);
unsigned char *u2cp(int, int);

#endif
