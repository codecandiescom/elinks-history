/* $Id: memlist.h,v 1.1 2002/03/18 06:33:13 pasky Exp $ */

#ifndef EL__UTIL_MEMLIST_H
#define EL__UTIL_MEMLIST_H

struct memory_list {
	int n;
	void *p[1];
};

struct memory_list *getml(void *, ...);
void add_to_ml(struct memory_list **, ...);
void freeml(struct memory_list *);

#endif
