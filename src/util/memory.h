/* $Id: memory.h,v 1.1 2002/06/17 07:42:32 pasky Exp $ */

#ifndef EL__UTIL_MEMORY_H
#define EL__UTIL_MEMORY_H

#define DUMMY ((void *) -1L)

#ifdef LEAK_DEBUG

#include "util/error.h"

#define mem_alloc(x) debug_mem_alloc(__FILE__, __LINE__, x)
#define mem_calloc(x, y) debug_mem_calloc(__FILE__, __LINE__, x, y)
#define mem_free(x) debug_mem_free(__FILE__, __LINE__, x)
#define mem_realloc(x, y) debug_mem_realloc(__FILE__, __LINE__, x, y)

#else

void *mem_alloc(size_t);
void *mem_calloc(size_t, size_t);
void mem_free(void *);
void *mem_realloc(void *, size_t);

static inline void *debug_mem_alloc(unsigned char *f, int l, size_t s) { return mem_alloc(s); }
static inline void debug_mem_free(unsigned char *f, int l, void *p) { mem_free(p); }
static inline void *debug_mem_realloc(unsigned char *f, int l, void *p, size_t s) { return mem_realloc(p, s); }

#endif

#endif
