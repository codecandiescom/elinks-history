/* $Id: charsets.h,v 1.1 2002/03/17 14:05:26 pasky Exp $ */

#ifndef EL__CHARSETS_H
#define EL__CHARSETS_H

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

#endif
