/* $Id: error.h,v 1.13 2003/06/08 12:19:15 pasky Exp $ */

#ifndef EL__UTIL_ERROR_H
#define EL__UTIL_ERROR_H

extern int errline;
extern unsigned char *errfile;

void list_magic_error_(unsigned char *, unsigned char *, unsigned char *, int);

void force_dump(void);
void do_not_optimize_here(void *);
void error(unsigned char *, ...);

void debug_msg(unsigned char *, ...);
void int_error(unsigned char *, ...);

/* This errfile thing is needed, as we don't have var-arg macros in standart,
 * only as gcc extension :(. */
#define internal errfile = __FILE__, errline = __LINE__, int_error
#define debug errfile = __FILE__, errline = __LINE__, debug_msg


/* This is our smart assert(). It is basically equivalent to if (x) internal(),
 * but it generates a uniform message and mainly does not do the test if we are
 * supposed to be lightning fast. Use it, use it much! */

#undef assert
#ifdef FASTMEM
#define assert(x) /* We don't do anything in FASTMEM mode. */
#else
#define assert(x) do { if (x) internal("assertion " #x " failed!"); } while (0)
#endif


#ifdef BACKTRACE
#include <stdio.h>
void dump_backtrace(FILE *, int);
#endif

#endif
