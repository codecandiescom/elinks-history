/* $Id: memdebug.h,v 1.1 2002/06/17 11:23:46 pasky Exp $ */

#ifndef EL__UTIL_MEMDEBUG_H
#define EL__UTIL_MEMDEBUG_H

#ifdef LEAK_DEBUG

/* TODO: Another file? */

extern long mem_amount;

void *debug_mem_alloc(unsigned char *, int, size_t);
void *debug_mem_calloc(unsigned char *, int, size_t, size_t);
void debug_mem_free(unsigned char *, int, void *);
void *debug_mem_realloc(unsigned char *, int, void *, size_t);
void set_mem_comment(void *, unsigned char *, int);

void check_memory_leaks();

#else

#include "util/memory.h"

static inline void *debug_mem_alloc(unsigned char *f, int l, size_t s) { return mem_alloc(s); }
static inline void debug_mem_free(unsigned char *f, int l, void *p) { mem_free(p); }
static inline void *debug_mem_realloc(unsigned char *f, int l, void *p, size_t s) { return mem_realloc(p, s); }

static inline void set_mem_comment(void *p, unsigned char *c, int l) {}

#endif

#endif
