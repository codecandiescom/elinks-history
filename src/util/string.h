/* $Id: string.h,v 1.1 2002/06/16 23:13:18 pasky Exp $ */

#ifndef EL__UTIL_STRING_H
#define EL__UTIL_STRING_H

unsigned char *debug_memacpy(unsigned char *, int, unsigned char *, int);
#define memacpy(s, l) debug_memacpy(__FILE__, __LINE__, s, l)

unsigned char *debug_stracpy(unsigned char *, int, unsigned char *);
#define stracpy(s) debug_stracpy(__FILE__, __LINE__, s)

unsigned char *copy_string(unsigned char **, unsigned char *);
void add_to_strn(unsigned char **, unsigned char *);
unsigned char *straconcat(unsigned char *, ...);

#define ALLOC_GR 0x100 /* must be power of 2 */
#define init_str() init_str_x(__FILE__, __LINE__)
unsigned char *init_str_x(unsigned char *, int);
void add_to_str(unsigned char **, int *, unsigned char *);
void add_bytes_to_str(unsigned char **, int *, unsigned char *, int);
void add_chr_to_str(unsigned char **, int *, unsigned char);
int xstrcmp(unsigned char *, unsigned char *);
int casecmp(unsigned char *, unsigned char *, int);

#ifndef HAVE_STRCASESTR
unsigned char *strcasestr(unsigned char *, unsigned char *);
#endif

unsigned char *safe_strncpy(unsigned char *, const unsigned char *, size_t);


#define WHITECHAR(x) ((x) == ' ' || ((x) >= 9 && (x) <= 13))

/* XXX: this macro should be renamed --Zas */
#define U(x) ((x) == '"' || (x) == '\'')

static inline int
isA(unsigned char c)
{
	return (c >= 'A' && c <= 'Z')
		|| (c >= 'a' && c <= 'z')
		|| (c >= '0' && c <= '9')
		|| c == '_' || c == '-';
}

#endif
