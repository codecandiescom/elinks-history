/* $Id: charsets.h,v 1.15 2004/05/15 23:40:14 zas Exp $ */

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
	CSM_NONE, /* Convert nothing. */
};

struct conv_table *get_translation_table(int, int);

/* The problem is that Samba (version 3.0.3), which provides libnss_wins.so.2,
 * has a function called convert_string(), which is called somewhere inside
 * _nss_wins_gethostbyname_r(). But, elinks also has a function called
 * convert_string() in src/intl/charsets.c that does something different.
 * This name clash causes the elinks hostname lookup thread to crash.
 * --Derek Poon */
/* FIXME: Filed as bug 453. convert_string() is now a macro,
 * wrapping elinks_convert_string(), it should fix this issue. --Zas */
#undef convert_string
unsigned char *elinks_convert_string(struct conv_table *convert_table, unsigned char *chars, int charslen, enum convert_string_mode mode);
#define convert_string(a,b,c,d) elinks_convert_string(a,b,c,d)

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
