/* This routines represent handling of struct memory_list. */
/* $Id: memlist.c,v 1.1 2002/03/18 06:33:13 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdarg.h>

#include <links.h>

#include <util/memlist.h>


/*
 * memory_list is used to track information about all allocated memory
 * belonging to something. Then we can free it when we won't need it
 * anymore, but the one who allocated it won't be able to get control
 * back in order to free it himself.
 */

struct memory_list *getml(void *p, ...)
{
	struct memory_list *ml;
	va_list ap;
	void *q;
	int i;

	va_start(ap, p);
	for (i = 0, q = p; q; i++) {
		q = va_arg(ap, void *);
	}

	ml = mem_alloc(sizeof(struct memory_list) + i * sizeof(void *));
	if (!ml) return NULL;

	ml->n = i;

	va_start(ap, p);
	for (i = 0, q = p; q; i++) {
		ml->p[i] = q;
		q = va_arg(ap, void *);
	}

	return ml;
}

void add_to_ml(struct memory_list **ml, ...)
{
	struct memory_list *nml;
	va_list ap;
	void *q;
	int n = 0;

	if (!*ml) {
		*ml = mem_alloc(sizeof(struct memory_list));
		if (!*ml) return;

		(*ml)->n = 0;
	}

	va_start(ap, ml);
	while ((q = va_arg(ap, void *))) n++;

	nml = mem_realloc(*ml, sizeof(struct memory_list) + (n + (*ml)->n) * sizeof(void *));
	if (!nml)
		return;

	va_start(ap, ml);
	while ((q = va_arg(ap, void *))) nml->p[nml->n++] = q;

	*ml = nml;
}

void freeml(struct memory_list *ml)
{
	int i;

	if (!ml) return;

	for (i = 0; i < ml->n; i++) mem_free(ml->p[i]);

	mem_free(ml);
}
