/* $Id: memlist.h,v 1.3 2003/06/07 08:37:21 zas Exp $ */

#ifndef EL__UTIL_MEMLIST_H
#define EL__UTIL_MEMLIST_H

struct memory_list {
	int n;
	void *p[1];
};

#undef DEBUG_MEMLIST

#if defined(DEBUG) && defined(HAVE_VARIADIC_MACROS)
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
