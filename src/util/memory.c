/* Memory allocation manager */
/* $Id: memory.c,v 1.16 2004/04/29 23:02:42 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#include <unistd.h>

#include "elinks.h"

#include "util/error.h"
#include "util/memory.h"


#if !defined(LEAK_DEBUG) && !defined(CONFIG_FASTMEM)

static int alloc_try = 0;

static int
patience(unsigned char *of)
{
	++alloc_try;
	if (alloc_try < ALLOC_MAXTRIES) {
		ERROR("Out of memory (%s returned NULL): retry #%d,"
			" I still exercise my patience and retry tirelessly.",
			of, alloc_try);
		sleep(ALLOC_DELAY);
		return alloc_try;
	}

#ifdef CRASH_IF_ALLOC_MAXTRIES
	INTERNAL("Out of memory (%s returned NULL) after %d tries,"
		" I give up. See ya on the other side.",
		of, alloc_try);
#else
	ERROR("Out of memory (%s returned NULL) after %d tries,"
		" I give up and try to continue. Pray for me, please.",
		of, alloc_try);
#endif
	alloc_try = 0;
	return 0;
}

void *
mem_alloc(size_t size)
{
	if (size)
		do {
			void *p = malloc(size);

			if (p) return p;
		} while (patience("malloc"));

	return NULL;
}

void *
mem_calloc(size_t count, size_t eltsize)
{
	if (eltsize && count)
		do {
			void *p = calloc(count, eltsize);

			if (p) return p;
		} while (patience("calloc"));

	return NULL;
}

void
mem_free(void *p)
{
	if (!p) {
		INTERNAL("mem_free(NULL)");
		return;
	}

	free(p);
}

void *
mem_realloc(void *p, size_t size)
{
	if (!p) return mem_alloc(size);

	if (size)
		do {
			void *p2 = realloc(p, size);

			if (p2) return p2;
		} while (patience("realloc"));
	else
		mem_free(p);

	return NULL;
}

#endif
