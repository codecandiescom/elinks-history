/* $Id: charsets.h,v 1.12 2003/11/21 22:23:05 zas Exp $ */

#ifndef EL__CHARSETS_H
#define EL__CHARSETS_H

#include "intl/codepage.h"
#include "util/types.h"

typedef uint32_t unicode_val;

/* UCS/Unicode replacement character. */
#define UCS_NO_CHAR ((unicode_val) 0xFFFD)

/* &nbsp; replacement character. See u2cp(). */
#define NBSP_CHAR ((unsigned char) 1)
#define NBSP_CHAR_STRING "\001"

struct conv_table {
	int t;
	union {
		unsigned char *str;
		struct conv_table *tbl;
	} u;
};

enum convert_string_mode {
	CSM_DEFAULT, /* Convert any char. */
	CSM_QUERY, /* Special handling of '&' and '=' chars. */
	CSM_FORM, /* Special handling of '&' and '=' chars in forms. */
};

struct conv_table *get_translation_table(int, int);
unsigned char *get_entity_string(const unsigned char *, const int, const int);
unsigned char *convert_string(struct conv_table *, unsigned char *, int, enum convert_string_mode mode);
int get_cp_index(unsigned char *);
unsigned char *get_cp_name(int);
unsigned char *get_cp_mime_name(int);
int is_cp_special(int);
void free_conv_table(void);
unsigned char *cp2utf_8(int, int);

unsigned char *u2cp_(unicode_val, int, int no_nbsp_hack);
#define u2cp(u, to) u2cp_(u, to, 0)
#define u2cp_no_nbsp(u, to) u2cp_(u, to, 1)

void init_charsets_lookup(void);
void free_charsets_lookup(void);

#endif
