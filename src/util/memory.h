/* $Id: memory.h,v 1.13 2003/09/10 18:12:54 jonas Exp $ */

#ifndef EL__UTIL_MEMORY_H
#define EL__UTIL_MEMORY_H

/* If defined, we'll crash if ALLOC_MAXTRIES is attained,
 * if not defined, we'll try to continue. */
/* #define CRASH_IF_ALLOC_MAXTRIES */
/* Max. number of retry in case of memory allocation failure. */
#define ALLOC_MAXTRIES 3
/* Delay in seconds between each alloc try. */
#define ALLOC_DELAY 1

#define fmem_alloc(x) mem_alloc(x)
#define fmem_free(x) mem_free(x)


#ifdef LEAK_DEBUG

#include "util/memdebug.h"

#define mem_alloc(x) debug_mem_alloc(__FILE__, __LINE__, x)
#define mem_calloc(x, y) debug_mem_calloc(__FILE__, __LINE__, x, y)
#define mem_free(x) debug_mem_free(__FILE__, __LINE__, x)
#define mem_realloc(x, y) debug_mem_realloc(__FILE__, __LINE__, x, y)

#else

#ifndef FASTMEM

/* Cygwin wants some size_t definition here... let's try to make it happy
 * then. Hrmpf. */
#include <sys/types.h>
#include <stddef.h>

void *mem_alloc(size_t);
void *mem_calloc(size_t, size_t);
void mem_free(void *);
void *mem_realloc(void *, size_t);

#else

# include <stdlib.h>

/* TODO: For enhanced portability, checks at configure time:
 * malloc(0) -> NULL
 * realloc(NULL, 0) -> NULL
 * realloc(p, 0) <-> free(p)
 * realloc(NULL, n) <-> malloc(n)
 * Some old implementations may not respect these rules.
 * For these we need some replacement functions.
 * This should not be an issue on most modern systems.
 */

# define mem_alloc(size) malloc(size)
# define mem_calloc(count, size) calloc(count, size)
# define mem_free(p) free(p)
# define mem_realloc(p, size) realloc(p, size)


/* fmem_* functions should be use for allocation and freeing of memory
 * inside a function.
 * See alloca(3) manpage. */

#undef fmem_alloc
#undef fmem_free

#ifdef HAVE_ALLOCA

#ifdef HAVE_ALLOCA_H
#include <alloca.h>
#endif
#define fmem_alloc(x) alloca(x)
#define fmem_free(x)

#else /* HAVE_ALLOCA */

#define fmem_alloc(x) mem_alloc(x)
#define fmem_free(x) mem_free(x)

#endif /* HAVE_ALLOCA */

#endif /* FASTMEM */

#endif /* LEAK_DEBUG */


/* Granular memory allocation. */

#include <string.h> /* for memset() */

#define grmask(x, gr)	((x) & ~((gr) - 1))

#define mem_gralloc(pointer, type, oldsize, newsize, gr)				\
	if (grmask(oldsize, gr) != grmask(newsize, gr)) {				\
		type *_tmp_ = (pointer);						\
		_tmp_ = mem_realloc(_tmp_, grmask((newsize) + (gr), gr) * sizeof(type));	\
		if (!_tmp_) return NULL;						\
		(pointer) = _tmp_;							\
	}

#ifdef MEM_ALIGN_SIZE
#undef MEM_ALIGN_SIZE
#endif

#define MEM_ALIGN_SIZE(x, gr) (((x) + (gr)) & ~(gr))

static inline void *
__mem_align_alloc(void **ptr, size_t old, size_t new, size_t objsize, int mask)
{
	size_t newsize = MEM_ALIGN_SIZE(new, mask);
	size_t oldsize = MEM_ALIGN_SIZE(old, mask);

	if (newsize > oldsize) {
		unsigned char *data = mem_realloc(*ptr, newsize * objsize);
		if (!data) return NULL;

		*ptr = (void *)data;
		memset(&data[oldsize * objsize], 0, (newsize - oldsize) * objsize);
	}

	return *ptr;
}

#define mem_align_alloc(ptr, old, new, objsize, mask) \
	__mem_align_alloc((void **)ptr, old, new, objsize, mask)

#endif
