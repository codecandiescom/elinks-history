/* $Id: error.h,v 1.21 2003/06/08 13:59:09 pasky Exp $ */

#ifndef EL__UTIL_ERROR_H
#define EL__UTIL_ERROR_H

void force_dump(void);


/* This function does nothing, except making compiler not to optimize certains
 * spots of code --- this is useful when that particular optimization is buggy.
 * So we are just workarounding buggy compilers. */
/* This function should be always used only in context of compiler version
 * specific macros. */
void do_not_optimize_here(void *);

#if defined(__GNUC__) && __GNUC__ == 2 && __GNUC_MINOR__ <= 7
#define do_not_optimize_here_gcc_2_7(x) do_not_optimize_here(x)
#else
#define do_not_optimize_here_gcc_2_7(x)
#endif


/* This errfile thing is needed, as we don't have var-arg macros in standart,
 * only as gcc extension :(. */
extern int errline;
extern unsigned char *errfile;

/* @debug(format_string) is used for printing of debugging information. It
 * should not be used anywhere in the official codebase (although it is often
 * lying there commented out, as it may get handy). */
#undef debug
#define debug errfile = __FILE__, errline = __LINE__, elinks_debug
void elinks_debug(unsigned char *, ...);

/* @error(format_string) is used to report non-fatal errors during the ELinks
 * run. It tries to (not that agressively) draw user's attention to the error,
 * but never dumps core or so. */
#undef error
#define error errfile = __FILE__, errline = __LINE__, elinks_error
void elinks_error(unsigned char *, ...);

/* @internal(format_string) is used to report fatal errors during the ELinks
 * run. It tries to draw user's attention to the error and dumps core if ELinks
 * is running in the DEBUG mode. */
#undef internal
#define internal errfile = __FILE__, errline = __LINE__, elinks_internal
void elinks_internal(unsigned char *, ...);


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
