/* These routines represent handling of struct memory_list. */
/* $Id: memlist.c,v 1.7 2003/06/04 23:23:18 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdarg.h>
#include <stdlib.h>

#include "elinks.h"

#include "util/memlist.h"
#include "util/memory.h"


/*
 * memory_list is used to track information about all allocated memory
 * belonging to something. Then we can free it when we won't need it
 * anymore, but the one who allocated it won't be able to get control
 * back in order to free it himself.
 */

#define ML_SIZE(n) (sizeof(struct memory_list) + (n) * sizeof(void *))

/* Create a memory list. If p is NULL or allocation fails, it will
 * returns NULL.
 * It always stops at first NULL element. */
struct memory_list *
getml(void *p, ...)
{
	struct memory_list *ml;
	va_list ap;
	void *q;
	int n = 1;

	/* If first element is NULL, there's no need to allocate memory, so
	 * just return. */
	if (!p) return NULL;

	/* How many elements ? */
	va_start(ap, p);
	while ((q = va_arg(ap, void *))) n++;
	va_end(ap);

	/* Allocate space for memory list. */
	ml = mem_alloc(ML_SIZE(n));
	if (!ml) return NULL;

	/* First element. */
	ml->n = 1;
	ml->p[0] = p;

	/* Next ones. */
	va_start(ap, p);
	while ((q = va_arg(ap, void *))) ml->p[ml->n++] = q;
	va_end(ap);

	/* The end. */
	return ml;
}

/* Add elements to a memory list.
 * If memory list exists, it enlarges it, else it creates it.
 * if there's no elements or first element is NULL, it does nothing.
 * It always stops at first NULL element. */
void
add_to_ml(struct memory_list **ml, ...)
{
	va_list ap;
	void *q;
	int n = 0;

	/* How many new elements ? */
	va_start(ap, ml);
	while ((q = va_arg(ap, void *))) n++;
	va_end(ap);

	/* None, so just return. */
	if (!n) return;

	if (!*ml) {
		/* If getml() wasn't called before or returned NULL,
		 * then we create it. */
		*ml = mem_alloc(ML_SIZE(n));
		if (!*ml) return;

		(*ml)->n = 0;
	} else {
		/* Enlarge existing ml. */
		struct memory_list *nml;

		nml = mem_realloc(*ml, ML_SIZE(n + (*ml)->n));
		if (!nml) return;

		*ml = nml;
	}

	/* Set ml with new elements and update count. */
	va_start(ap, ml);
	while ((q = va_arg(ap, void *))) (*ml)->p[(*ml)->n++] = q;
	va_end(ap);
}

/* Free elements and memory list.
 * It ignores NULL pointers. */
void
freeml(struct memory_list *ml)
{
	if (ml) {
		int i;

		for (i = 0; i < ml->n; i++)
			mem_free(ml->p[i]);

		mem_free(ml);
	}
}

#undef ML_SIZE
