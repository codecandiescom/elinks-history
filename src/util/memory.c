/* Memory allocation manager */
/* $Id: memory.c,v 1.19 2004/09/23 13:12:04 zas Exp $ */

#define _GNU_SOURCE /* MREMAP_MAYMOVE */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdlib.h>
#ifdef HAVE_MMAP
#include <sys/mman.h>
#endif
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


/* TODO: Leak detector and the usual protection gear? patience()?
 *
 * We could just alias mem_mmap_* to mem_debug_* #if LEAK_DEBUG, *WHEN* we are
 * confident that the mmap() code is really bugless ;-). --pasky */

#ifdef HAVE_MMAP

static int page_size;

/* This tries to prevent useless reallocations, especially since they are quite
 * expensive in the mremap()-less case. */
static size_t
round_size(size_t size)
{
	if (!page_size) page_size = sysconf(_SC_PAGE_SIZE);
	if (!page_size || page_size == -1) page_size = 4096;
	return (size / page_size + 1) * page_size;
}

/* Some systems may not have MAP_ANON but MAP_ANONYMOUS instead. */
#if defined(MAP_ANONYMOUS) && !defined(MAP_ANON)
#define MAP_ANON MAP_ANONYMOUS
#endif

void *
mem_mmap_alloc(size_t size)
{
	if (size) {
		void *p = mmap(NULL, round_size(size), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);

		return p;
	}

	return NULL;
}

void
mem_mmap_free(void *p, size_t size)
{
	if (!p) {
		INTERNAL("mem_mmap_free(NULL)");
		return;
	}

	munmap(p, round_size(size));
}

void *
mem_mmap_realloc(void *p, size_t old_size, size_t new_size)
{
	if (!p) return mem_mmap_alloc(new_size);

	if (round_size(old_size) == round_size(new_size))
		return p;

	if (new_size) {
#ifdef HAVE_MREMAP
		void *p2 = mremap(p, round_size(old_size), round_size(new_size), MREMAP_MAYMOVE);

		return p2;
#else
		void *p2 = mem_mmap_alloc(new_size);

		if (p2) memcpy(p2, p, old_size);
		munmap(p, round_size(old_size));
		return p2;
#endif
	} else {
		mem_mmap_free(p, old_size);
	}

	return NULL;
}

#endif
