/* Memory allocation manager */
/* $Id: memory.c,v 1.4 2002/11/25 12:04:56 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>

#include "links.h"

#include "util/error.h"
#include "util/memory.h"


#ifndef LEAK_DEBUG

int alloc_try = 0;

void *
mem_alloc(size_t size)
{
	void *p;

	if (!size) return DUMMY;

again:
	p = malloc(size);
	if (!p) {
		++alloc_try;
		if (alloc_try < ALLOC_MAXTRIES) {
			fprintf(stderr, "out of memory (malloc returned NULL) retry %d,"
					" i wait and retry.", alloc_try);
			sleep(ALLOC_DELAY);
			goto again;
		} else {
			alloc_try = 0;
#ifdef CRASH_IF_ALLOC_MAXTRIES
			internal(stderr, "out of memory (malloc returned NULL) after %d tries,"
					 " i give up.", alloc_try);
#else
			fprintf(stderr, "out of memory (malloc returned NULL) after %d tries,"
					" i give up and try to continue.", alloc_try);
#endif
		}
	}

	return p;
}

void *
mem_calloc(size_t count, size_t eltsize)
{
	void *p;

	if (!eltsize || !count) return DUMMY;

again:
	p = calloc(count, eltsize);
	if (!p) {
		++alloc_try;
		if (alloc_try < ALLOC_MAXTRIES) {
			fprintf(stderr, "out of memory (calloc returned NULL) retry %d,"
					" i wait and retry.", alloc_try);
			sleep(ALLOC_DELAY);
			goto again;
		} else {
			alloc_try = 0;
#ifdef CRASH_IF_ALLOC_MAXTRIES
			internal(stderr, "out of memory (calloc returned NULL) after %d tries,"
					 " i give up.", alloc_try);
#else
			fprintf(stderr, "out of memory (calloc returned NULL) after %d tries,"
					" i give up and try to continue.", alloc_try);
#endif
		}
	}

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

again:
	p = realloc(p, size);
	if (!p) {
		++alloc_try;
		if (alloc_try < ALLOC_MAXTRIES) {
			fprintf(stderr, "out of memory (realloc returned NULL) retry %d,"
					" i wait and retry.", alloc_try);
			sleep(ALLOC_DELAY);
			goto again;
		} else {
			alloc_try = 0;
#ifdef CRASH_IF_ALLOC_MAXTRIES
			internal(stderr, "out of memory (realloc returned NULL) after %d tries,"
					 " i give up.", alloc_try);
#else
			fprintf(stderr, "out of memory (realloc returned NULL) after %d tries,"
					" i give up and try to continue.", alloc_try);
#endif
		}
	}

	return p;
}

#endif /* LEAK_DEBUG */
