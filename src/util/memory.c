/* Memory allocation manager */
/* $Id: memory.c,v 1.5 2002/11/28 22:45:08 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include "links.h"

#include "util/error.h"
#include "util/memory.h"


#ifndef LEAK_DEBUG

int alloc_try = 0;

int
patience(unsigned char *of)
{
	++alloc_try;
	if (alloc_try < ALLOC_MAXTRIES) {
		error("Out of memory (%s returned NULL): retry #%d,"
			" I still exercise my patience and retry tirelessly.",
			of, alloc_try);
		sleep(ALLOC_DELAY);
		return alloc_try;
	}

#ifdef CRASH_IF_ALLOC_MAXTRIES
	internal("Out of memory (%s returned NULL) after %d tries,"
		" I give up. See ya on the other side.",
		of, alloc_try);
#else
	error("Out of memory (%s returned NULL) after %d tries,"
		" I give up and try to continue. Pray for me, please.",
		of, alloc_try);
#endif
	alloc_try = 0;
	return 0;
}

void *
mem_alloc(size_t size)
{
	void *p = NULL;

	if (!size) return DUMMY;

	do {
		p = malloc(size);
		if (p) break;
	} while (patience("malloc"));

	return p;
}

void *
mem_calloc(size_t count, size_t eltsize)
{
	void *p;

	if (!eltsize || !count) return DUMMY;

	do {
		p = calloc(count, eltsize);
		if (p) break;
	} while (patience("calloc"));

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
	if (!p || p == DUMMY) return mem_alloc(size);

	if (!size) {
		mem_free(p);
		return DUMMY;
	}

	do {
		p = realloc(p, size);
		if (p) break;
	} while (patience("realloc"));

	return p;
}

#endif /* LEAK_DEBUG */
