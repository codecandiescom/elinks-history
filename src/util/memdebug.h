/* $Id: memdebug.h,v 1.2 2002/09/11 18:37:33 zas Exp $ */

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
#define set_mem_comment(p, c, l)
#endif /* LEAK_DEBUG */

#endif
