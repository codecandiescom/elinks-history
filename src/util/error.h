/* $Id: error.h,v 1.16 2003/06/08 12:36:56 pasky Exp $ */

#ifndef EL__UTIL_ERROR_H
#define EL__UTIL_ERROR_H

void list_magic_error_(unsigned char *, unsigned char *, unsigned char *, int);

void force_dump(void);
void do_not_optimize_here(void *);


/* This errfile thing is needed, as we don't have var-arg macros in standart,
 * only as gcc extension :(. */
extern int errline;
extern unsigned char *errfile;

/* @internal(format_string) is used to report fatal errors during the ELinks
 * run. It tries to draw user's attention to the error and dumps core if ELinks
 * is running in the DEBUG mode. */
#define internal errfile = __FILE__, errline = __LINE__, elinks_internal
void elinks_internal(unsigned char *, ...);

/* @error(format_string) is used to report non-fatal errors during the ELinks
 * run. It tries to (not that agressively) draw user's attention to the error,
 * but never dumps core or so. */
#define error elinks_error
void elinks_error(unsigned char *, ...);

/* @debug(format_string) is used for printing of debugging information. It
 * should not be used anywhere in the official codebase (although it is often
 * lying there commented out, as it may get handy). */
#define debug errfile = __FILE__, errline = __LINE__, elinks_debug
void elinks_debug(unsigned char *, ...);


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
