/* $Id: memory.h,v 1.7 2002/12/23 21:36:09 pasky Exp $ */

#ifndef EL__UTIL_MEMORY_H
#define EL__UTIL_MEMORY_H

/* If defined, we'll crash if ALLOC_MAXTRIES is attained,
 * if not defined, we'll try to continue. */
/* #define CRASH_IF_ALLOC_MAXTRIES */
/* Max. number of retry in case of memory allocation failure. */
#define ALLOC_MAXTRIES 3
/* Delay in seconds between each alloc try. */
#define ALLOC_DELAY 1


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

#endif /* FASTMEM */

#endif /* LEAK_DEBUG */

#endif
