/* $Id: memlist.h,v 1.2 2003/06/05 16:39:25 zas Exp $ */

#ifndef EL__UTIL_MEMLIST_H
#define EL__UTIL_MEMLIST_H

struct memory_list {
	int n;
	void *p[1];
};

#undef DEBUG_MEMLIST
/* variadic macros are available in compilers conforming to the ISO C99 standard
 * We test stdc version and enable memlist debug only if a recent one is
 * found. Not perfect but should work. --Zas */
#if defined(DEBUG) && \
    ((defined(__GNUC__) && __GNUC__ >= 2) || \
     (defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L))
struct memory_list *debug_getml(unsigned char *file, int line, void *p, ...);
void debug_add_to_ml(unsigned char *file, int line, struct memory_list **ml, ...);
#define getml(...) debug_getml(__FILE__, __LINE__, __VA_ARGS__)
#define add_to_ml(...) debug_add_to_ml(__FILE__, __LINE__, __VA_ARGS__)
#define DEBUG_MEMLIST
#else
struct memory_list *getml(void *p, ...);
void add_to_ml(struct memory_list **ml, ...);
#endif

void freeml(struct memory_list *);

#endif
