/* Memory allocation manager */
/* $Id: memory.c,v 1.2 2002/06/21 19:25:09 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include "links.h"

#include "util/error.h"
#include "util/memory.h"

#ifndef LEAK_DEBUG

void *
mem_alloc(size_t size)
{
	void *p;

	if (!size) return DUMMY;

	p = malloc(size);
	if (!p) error("ERROR: out of memory (malloc returned NULL)\n");

	return p;
}

void *
mem_calloc(size_t count, size_t eltsize)
{
	void *p;

	if (!eltsize || !count) return DUMMY;

	p = calloc(count, eltsize);
	if (!p) error("ERROR: out of memory (calloc returned NULL)\n");

	return p;
}

void
mem_free(void *p)
{
	if (p == DUMMY) return;

	if (!p) {
		internal("mem_free(NULL)");
		return;
	}

	free(p);
}

void *
mem_realloc(void *p, size_t size)
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

	p = realloc(p, size);
	if (!p) error("ERROR: out of memory (realloc returned NULL)\n");

	return p;
}

#endif /* LEAK_DEBUG */
