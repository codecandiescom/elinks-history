/* $Id: error.h,v 1.2 2002/03/16 22:03:09 pasky Exp $ */

#ifndef EL__ERROR_H
#define EL__ERROR_H

void do_not_optimize_here(void *);
void error(unsigned char *, ...);

extern int errline;
extern unsigned char *errfile;
void debug_msg(unsigned char *, ...);
void int_error(unsigned char *, ...);

/* This errfile thing is needed, as we don't have var-arg macros in standart,
 * only as gcc extension :(. */
#define internal errfile = __FILE__, errline = __LINE__, int_error
#define debug errfile = __FILE__, errline = __LINE__, debug_msg

#ifdef LEAK_DEBUG

/* TODO: Another file? */

extern long mem_amount;
extern long last_mem_amount;

void *debug_mem_alloc(unsigned char *, int, size_t);
void debug_mem_free(unsigned char *, int, void *);
void *debug_mem_realloc(unsigned char *, int, void *, size_t);
void set_mem_comment(void *, unsigned char *, int);

void check_memory_leaks();

#else

static inline void set_mem_comment(void *p, unsigned char *c, int l) {}

#endif

#endif
