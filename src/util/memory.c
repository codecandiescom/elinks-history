/* Memory allocation manager */
/* $Id: memory.c,v 1.9 2002/11/29 20:59:53 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <unistd.h>

#include "links.h"

#include "util/error.h"
#include "util/memory.h"


#ifndef LEAK_DEBUG

static int alloc_try = 0;

static int
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

inline void *
mem_alloc(size_t size)
{
	void *p;

	if (!size) return NULL;

	do {
		p = malloc(size);
		if (p) break;
	} while (patience("malloc"));

	return p;
}

inline void *
mem_calloc(size_t count, size_t eltsize)
{
	void *p;

	if (!eltsize || !count) return NULL;

	do {
		p = calloc(count, eltsize);
		if (p) break;
	} while (patience("calloc"));

	return p;
}

inline void
mem_free(void *p)
{
	if (!p) {
		internal("mem_free(NULL)");
		return;
	}

	free(p);
}

inline void *
mem_realloc(void *p, size_t size)
{
	void *p2;

	if (!p) return mem_alloc(size);

	if (!size) {
		mem_free(p);
		return NULL;
	}

	do {
		p2 = realloc(p, size);
		if (p2) {
			p = p2;
			break;
		}
	} while (patience("realloc"));
	if (!p2) return NULL;

	return p;
}

#endif /* LEAK_DEBUG */
