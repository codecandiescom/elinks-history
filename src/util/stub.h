/* $Id: stub.h,v 1.1 2003/07/22 15:59:19 jonas Exp $ */

#ifndef EL__UTIL_STUB_H
#define EL__UTIL_STUB_H

#if 0
#ifdef DEBUG
#define USE_OWN_LIBC
#endif
#endif

#ifdef USE_OWN_LIBC

#undef HAVE_MEMMOVE
#undef HAVE_BCOPY /* prevent using bcopy() stub for memmove() */
#undef HAVE_MEMPCPY
#undef HAVE_STPCPY
#undef HAVE_STRCASECMP
#undef HAVE_STRCASESTR
#undef HAVE_STRDUP
#undef HAVE_STRERROR
#undef HAVE_STRNCASECMP
#undef HAVE_STRSTR

#endif /* USE_OWN_LIBC */

/** strchr() */

#ifndef HAVE_STRCHR
#ifdef HAVE_INDEX /* for old BSD systems. */

#undef strchr
#define strchr(a, b) index(a, b)
#undef strrchr
#define strrchr(a, b) rindex(a, b)

#else /* ! HAVE_INDEX */
# error You have neither strchr() nor index() function. Please go upgrade your system.
#endif /* HAVE_INDEX */
#endif /* HAVE_STRCHR */

/** strerror() */
#ifndef HAVE_STRERROR
#undef strerror
#define strerror(e) elinks_strerror(e)
const char *elinks_strerror(int);
#endif

/** strstr() */
#ifndef HAVE_STRSTR
#undef strstr
#define strstr(a, b) elinks_strstr(a, b)
char *elinks_strstr(const char *, const char *);
#endif

/** memmove() */
#ifndef HAVE_MEMMOVE
#ifdef HAVE_BCOPY
# define memmove(dst, src, n) bcopy(src, dst, n)
#else
#undef memmove
#define memmove(dst, src, n) elinks_memmove(dst, src, n)
void *elinks_memmove(void *, const void *, size_t);
#endif
#endif

/** strcasecmp() */
#ifndef HAVE_STRCASECMP
#undef strcasecmp
#define strcasecmp(a, b) elinks_strcasecmp(a, b)
int elinks_strcasecmp(const unsigned char *, const unsigned char *);
#endif

/** strncasecmp() */
#ifndef HAVE_STRNCASECMP
#undef strncasecmp
#define strncasecmp(a, b, l) elinks_strncasecmp(a, b, l)
int elinks_strncasecmp(const unsigned char *, const unsigned char *, size_t);
#endif

/** strcasestr() */
#ifndef HAVE_STRCASESTR
#undef strcasestr
#define strcasestr(a, b) elinks_strcasestr(a, b)
unsigned char *elinks_strcasestr(const unsigned char *, const unsigned char *);
#endif

/** strdup() */
#ifndef HAVE_STRDUP
#undef strdup
#define strdup(s) elinks_strdup(s)
unsigned char *elinks_strdup(const unsigned char *);
#endif

/* stpcpy() */
#ifndef HAVE_STPCPY
#undef stpcpy
#define stpcpy(d, s) elinks_stpcpy(d, s)
unsigned char *elinks_stpcpy(unsigned char *, unsigned const char *);
#endif

/* mempcpy() */
#ifndef HAVE_MEMPCPY
#undef mempcpy
#define mempcpy(dest, src, n) elinks_mempcpy(dest, src, n)
void *elinks_mempcpy(void *, const void *, size_t);
#endif

#endif
