/* Global include with common functions and definitions for elinks */
/* $Id: links.h,v 1.61 2002/03/16 22:03:09 pasky Exp $ */

#ifndef EL__LINKS_H
#define EL__LINKS_H

#ifndef __EXTENSION__
#define __EXTENSION__ /* Helper for SunOS */
#endif

/* Includes for internal functions */

#ifdef HAVE_LIMITS_H
#include <limits.h>
#endif
#include <stdlib.h>
#include <string.h>

/* Global includes, which we unfortunately still need, because later we'll
 * define own ill-named aggressive macros which will completely ruin those
 * header files. */

#ifdef HAVE_SSL
#include <openssl/ssl.h>
#endif

#ifdef HAVE_LUA
#include <lua.h>
#include <lualib.h>
#endif


#include "os_dep.h"

#if 0

/* These are residuum from old links - dunno what they were meant for? */

#ifdef HAVE_SYS_RESOURCE_H
#  include <sys/resource.h>
#endif

#ifdef HAVE_NETINET_IN_SYSTM_H
#  include <netinet/in_systm.h>
#else
#  ifdef HAVE_NETINET_IN_SYSTEM_H
#    include <netinet/in_system.h>
#  endif
#endif

#endif

#include "os_depx.h"

#include "setup.h"


/* Includes for internal functions */

#include "error.h"


#define DUMMY ((void *) -1L)

#define MAX_STR_LEN     1024


/* Misc. types definition */

typedef unsigned tcount;


/* Memory managment */

#if !defined(LEAK_DEBUG) && defined(LEAK_DEBUG_LIST)
#error "You defined LEAK_DEBUG_LIST, but not LEAK_DEBUG!"
#endif

#ifdef LEAK_DEBUG

/* XXX: No, I don't like this too. Probably we should have the leak debugger
 * in separate file. --pasky */

#include "error.h"

#define mem_alloc(x) debug_mem_alloc(__FILE__, __LINE__, x)
#define mem_free(x) debug_mem_free(__FILE__, __LINE__, x)
#define mem_realloc(x, y) debug_mem_realloc(__FILE__, __LINE__, x, y)

#else

static inline void *mem_alloc(size_t size)
{
	void *p;
	if (!size) return DUMMY;
	if (!(p = malloc(size))) {
		error("ERROR: out of memory (malloc returned NULL)\n");
		return NULL;
	}
	return p;
}

static inline void mem_free(void *p)
{
	if (p == DUMMY) return;
	if (!p) {
		internal("mem_free(NULL)");
		return;
	}
	free(p);
}

static inline void *mem_realloc(void *p, size_t size)
{
	if (p == DUMMY) return mem_alloc(size);
	if (!p) {
		internal("mem_realloc(NULL, %d)", size);
		return NULL;
	}
	if (!size) {
		mem_free(p);
		return DUMMY;
	}
	if (!(p = realloc(p, size))) {
		error("ERROR: out of memory (realloc returned NULL)\n");
		return NULL;
	}
	return p;
}

#endif

#if !(defined(LEAK_DEBUG) && defined(LEAK_DEBUG_LIST))

static inline unsigned char *memacpy(const unsigned char *src, int len)
{
	unsigned char *m;
	if ((m = mem_alloc(len + 1))) {
		memcpy(m, src, len);
		m[len] = 0;
	}
	return m;
}

static inline unsigned char *stracpy(const unsigned char *src)
{
	return src ? memacpy(src, src != DUMMY ? strlen(src) : 0) : NULL;
}

#else

static inline unsigned char *debug_memacpy(unsigned char *f, int l, unsigned char *src, int len)
{
	unsigned char *m;
	if ((m = debug_mem_alloc(f, l, len + 1))) {
		memcpy(m, src, len);
		m[len] = 0;
	}
	return m;
}

#define memacpy(s, l) debug_memacpy(__FILE__, __LINE__, s, l)

static inline unsigned char *debug_stracpy(unsigned char *f, int l, unsigned char *src)
{
	return src ? debug_memacpy(f, l, src, src != DUMMY ? strlen(src) : 0) : NULL;
}

#define stracpy(s) debug_stracpy(__FILE__, __LINE__, s)

#endif


/* Inline utility functions */

static inline unsigned char upcase(unsigned char a)
{
	if (a>='a' && a<='z') a -= 0x20;
	return a;
}

static inline int xstrcmp(unsigned char *s1, unsigned char *s2)
{
        if (!s1 && !s2) return 0;
        if (!s1) return -1;
        if (!s2) return 1;
        return strcmp(s1, s2);
}

static inline int cmpbeg(unsigned char *str, unsigned char *b)
{
	while (*str && upcase(*str) == upcase(*b)) str++, b++;
	return !!*b;
}

static inline int snprint(unsigned char *s, int n, unsigned num)
{
	int q = 1;
	while (q <= num / 10) q *= 10;
	while (n-- > 1 && q) *(s++) = num / q + '0', num %= q, q /= 10;
	*s = 0;
	return !!q;
}

static inline int snzprint(unsigned char *s, int n, int num)
{
	if (n > 1 && num < 0) *(s++) = '-', num = -num, n--;
	return snprint(s, n, num);
}

static inline void add_to_strn(unsigned char **s, unsigned char *a)
{
	unsigned char *p;
	if (!(p = mem_realloc(*s, strlen(*s) + strlen(a) + 1))) return;
	strcat(p, a);
	*s = p;
}

#define ALLOC_GR	0x100		/* must be power of 2 */

#define init_str() init_str_x(__FILE__, __LINE__)

static inline unsigned char *init_str_x(unsigned char *file, int line)
{
	unsigned char *p;
	if ((p = debug_mem_alloc(file, line, ALLOC_GR))) *p = 0;
	return p;
}

static inline void add_to_str(unsigned char **s, int *l, unsigned char *a)
{
	unsigned char *p;
	int ll = strlen(a);
	if ((*l & ~(ALLOC_GR - 1)) != ((*l + ll) & ~(ALLOC_GR - 1)) &&
	   (!(p = mem_realloc(*s, (*l + ll + ALLOC_GR) & ~(ALLOC_GR - 1))) ||
	   !(*s = p))) return;
	strcpy(*s + *l, a); *l += ll;
}

static inline void add_bytes_to_str(unsigned char **s, int *l, unsigned char *a, int ll)
{
	unsigned char *p;
	if ((*l & ~(ALLOC_GR - 1)) != ((*l + ll) & ~(ALLOC_GR - 1)) &&
	   (!(p = mem_realloc(*s, (*l + ll + ALLOC_GR) & ~(ALLOC_GR - 1))) ||
	   !(*s = p))) return;
	memcpy(*s + *l, a, ll); (*s)[*l += ll] = 0;
}

static inline void add_chr_to_str(unsigned char **s, int *l, unsigned char a)
{
	unsigned char *p;
	if ((*l & (ALLOC_GR - 1)) == ALLOC_GR - 1 &&
	   (!(p = mem_realloc(*s, (*l + 1 + ALLOC_GR) & ~(ALLOC_GR - 1))) ||
	   !(*s = p))) return;
	*(*s + *l) = a; *(*s + ++(*l)) = 0;
}

static inline void add_num_to_str(unsigned char **s, int *l, int n)
{
	unsigned char a[64];
	/*sprintf(a, "%d", n);*/
	snzprint(a, 64, n);
	add_to_str(s, l, a);
}

static inline void add_knum_to_str(unsigned char **s, int *l, int n)
{
	unsigned char a[13];
	if (n && n / (1024 * 1024) * (1024 * 1024) == n) snzprint(a, 12, n / (1024 * 1024)), strcat(a, "M");
	else if (n && n / 1024 * 1024 == n) snzprint(a, 12, n / 1024), strcat(a, "k");
	else snzprint(a, 13, n);
	add_to_str(s, l, a);
}

static inline long strtolx(unsigned char *c, unsigned char **end)
{
	long l = strtol(c, (char **)end, 10);
	if (!*end) return l;
	if (upcase(**end) == 'K') {
		(*end)++;
		if (l < -MAXINT / 1024) return -MAXINT;
		if (l > MAXINT / 1024) return MAXINT;
		return l * 1024;
	}
	if (upcase(**end) == 'M') {
		(*end)++;
		if (l < -MAXINT / (1024 * 1024)) return -MAXINT;
		if (l > MAXINT / (1024 * 1024)) return MAXINT;
		return l * (1024 * 1024);
	}
	return l;
}

static inline unsigned char *copy_string(unsigned char **dst, unsigned char *src)
{
	if ((*dst = src) && (*dst = mem_alloc(strlen(src) + 1))) strcpy(*dst, src);
	return *dst;
}

/* Copies at most dst_size chars into dst. Ensures null termination of dst. */
static inline unsigned char *safe_strncpy(unsigned char *dst, const unsigned char *src, size_t dst_size) {
#if 0
	size_t to_copy;

	to_copy = strlen(src);

	/* Ensure that the url size is not greater than str_size */
	if (dst_size < to_copy)
		to_copy = dst_size - 1;

	strncpy(dst, src, to_copy);

	/* Ensure null termination */
	dst[to_copy] = '\0';
	
	return dst;
#endif

	strncpy(dst, src, dst_size);
	dst[dst_size - 1] = 0;
	
	return dst;
}


struct list_head {
	void *next;
	void *prev;
};

#ifndef HAVE_TYPEOF

struct xlist_head {
	struct xlist_head *next;
	struct xlist_head *prev;
};

#endif

#define init_list(x) {(x).next=&(x); (x).prev=&(x);}
#define list_empty(x) ((x).next == &(x))
#define del_from_list(x) {((struct list_head *)(x)->next)->prev=(x)->prev; ((struct list_head *)(x)->prev)->next=(x)->next;}
/*#define add_to_list(l,x) {(x)->next=(l).next; (x)->prev=(typeof(x))&(l); (l).next=(x); if ((l).prev==&(l)) (l).prev=(x);}*/
#define add_at_pos(p,x) do {(x)->next=(p)->next; (x)->prev=(p); (p)->next=(x); (x)->next->prev=(x);} while(0)

#ifdef HAVE_TYPEOF
#define add_to_list(l,x) add_at_pos((typeof(x))&(l),(x))
#define foreach(e,l) for ((e)=(l).next; (e)!=(typeof(e))&(l); (e)=(e)->next)
#define foreachback(e,l) for ((e)=(l).prev; (e)!=(typeof(e))&(l); (e)=(e)->prev)
#else
#define add_to_list(l,x) add_at_pos((struct xlist_head *)&(l),(struct xlist_head *)(x))
#define foreach(e,l) for ((e)=(l).next; (e)!=(void *)&(l); (e)=(e)->next)
#define foreachback(e,l) for ((e)=(l).prev; (e)!=(void *)&(l); (e)=(e)->prev)
#endif
#define free_list(l) {while ((l).next != &(l)) {struct list_head *a=(l).next; del_from_list(a); mem_free(a); }}

#define WHITECHAR(x) ((x) == 9 || (x) == 10 || (x) == 12 || (x) == 13 || (x) == ' ')
#define U(x) ((x) == '"' || (x) == '\'')

static inline int isA(unsigned char c)
{
	return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
	        c == '_' || c == '-';
}

static inline int casecmp(unsigned char *c1, unsigned char *c2, int len)
{
	int i;
	for (i = 0; i < len; i++) if (upcase(c1[i]) != upcase(c2[i])) return 1;
	return 0;
}

#ifndef HAVE_STRCASESTR
/* Stub for strcasestr(), GNU extension */
static inline unsigned char *strcasestr(unsigned char *haystack, unsigned char *needle) {
	size_t haystack_length = strlen(haystack);
	size_t needle_length = strlen(needle);
	int i;

	if (haystack_length < needle_length)
		return NULL;

	for (i = haystack_length - needle_length + 1; i; i--) {
		if (!casecmp(haystack, needle, needle_length))
			return haystack;
		haystack++;
	}

	return NULL;
}
#endif

/* *_info() types */
/* TODO: Move to.. somewhere :). */
#define CI_BYTES	1
#define CI_FILES	2
#define CI_LOCKED	3
#define CI_LOADING	4
#define CI_TIMERS	5
#define CI_TRANSFER	6
#define CI_CONNECTING	7
#define CI_KEEP		8
#define CI_LIST		9


#endif
