/* $Id: error.h,v 1.29 2003/07/06 20:28:43 pasky Exp $ */

#ifndef EL__UTIL_ERROR_H
#define EL__UTIL_ERROR_H


/* Here you will found a chunk of functions useful for error states --- from
 * reporting of various problems to generic error tests/workarounds to some
 * tools to be used when you got into an error state already. Some of the
 * functions are also useful for debugging. */


/* This errfile thing is needed, as we don't have var-arg macros in standart,
 * only as gcc extension :(. */
extern int errline;
extern unsigned char *errfile;

/* @debug(format_string) is used for printing of debugging information. It
 * should not be used anywhere in the official codebase (although it is often
 * lying there commented out, as it may get handy). */
#undef debug
#define debug errfile = __FILE__, errline = __LINE__, elinks_debug
void elinks_debug(unsigned char *fmt, ...);

/* @error(format_string) is used to report non-fatal errors during the ELinks
 * run. It tries to (not that agressively) draw user's attention to the error,
 * but never dumps core or so. */
#undef error
#define error errfile = __FILE__, errline = __LINE__, elinks_error
void elinks_error(unsigned char *fmt, ...);

/* @internal(format_string) is used to report fatal errors during the ELinks
 * run. It tries to draw user's attention to the error and dumps core if ELinks
 * is running in the DEBUG mode. */
#undef internal
#define internal errfile = __FILE__, errline = __LINE__, elinks_internal
void elinks_internal(unsigned char *fmt, ...);


/* This is our smart assert(). It is basically equivalent to if (x) internal(),
 * but it generates a uniform message and mainly does not do the test if we are
 * supposed to be lightning fast. Use it, use it a lot! */

/* To make recovery path possible (assertion failed may not mean end of the
 * world, the execution goes on if we're outside of DEBUG and FASTMEM),
 * @assert_failed is set to true if the last assert() failed, otherwise it's
 * zero. Note that you must never change assert_failed value, sorry guys. */

/* In-depth explanation: this restriction is here because in the FASTMEM mode,
 * assert_failed is initially initialized to zero and then not ever touched
 * anymore. So if you change it to non-zero failure, your all further recovery
 * paths will get hit (and since developers usually don't test FASTMEM mode
 * extensively...). So better don't mess with it, even if you would do that
 * with awareness of this fact. We don't want to iterate over tens of spots all
 * over the code when we chane one detail regarding FASTMEM operation. */

extern int assert_failed;

#undef assert
#ifdef FASTMEM
#define assert(x) /* We don't do anything in FASTMEM mode. */
#else
#define assert(x) \
do { if ((assert_failed = !(x))) { \
	internal("assertion " #x " failed!"); \
} } while (0)
#endif

/* This is extended assert() version, it can print additional user-specified
 * message. Quite useful not only to detect that _something_ is wrong, but also
 * _how_ wrong is it ;-). Note that the format string must always be a regular
 * string, not a variable reference. Also, be careful _what_ will you attempt
 * to print, or you could easily get just a SIGSEGV instead of the assertion
 * failed message. */

#undef assertm
#ifdef HAVE_VARIADIC_MACROS
#ifdef FASTMEM
#define assertm(x,m...) /* We don't do anything in FASTMEM mode. */
#else
#define assertm(x,m...) \
do { if ((assert_failed = !(x))) { \
	internal("assertion " #x " failed: " m); \
} } while (0)
#endif
#else /* HAVE_VARIADIC_MACROS */
#ifdef FASTMEM
#define assertm elinks_assertm
#else
#define assertm errfile = __FILE__, errline = __LINE__, elinks_assertm
#endif
/* This is not nice at all, and does not really work that nice as macros do
 * But it is good to try to do at least _some_ assertm()ing even when the
 * variadic macros are not supported. */
/* XXX: assertm() usage could generate warnings (we assume that the assert()ed
 * expression is int (and that's completely fine, I do *NOT* want to see any
 * stinking assert((int)pointer) ! ;-)), so DEBUG (-Werror) and
 * !HAVE_VARIADIC_MACROS won't play well together. Hrm. --pasky */
#ifdef FASTMEM
static inline
#endif
void elinks_assertm(int x, unsigned char *fmt, ...)
#ifdef FASTMEM
{
	/* We don't do anything in FASTMEM mode. Let's hope that the compiler
	 * will at least optimize out the @x computation. */
}
#else
	;
#endif
#endif /* HAVE_VARIADIC_MACROS */


/* This will print some fancy message, version string and possibly do something
 * else useful. Then, it will dump core. */
#ifdef DEBUG
void force_dump(void);
#endif


/* This function does nothing, except making compiler not to optimize certains
 * spots of code --- this is useful when that particular optimization is buggy.
 * So we are just workarounding buggy compilers. */
/* This function should be always used only in context of compiler version
 * specific macros. */
void do_not_optimize_here(void *x);

#if defined(__GNUC__) && __GNUC__ == 2 && __GNUC_MINOR__ <= 7
#define do_not_optimize_here_gcc_2_7(x) do_not_optimize_here(x)
#else
#define do_not_optimize_here_gcc_2_7(x)
#endif

#if defined(__GNUC__) && __GNUC__ == 3 && __GNUC_MINOR__ == 3
#define do_not_optimize_here_gcc_3_3(x) do_not_optimize_here(x)
#else
#define do_not_optimize_here_gcc_3_3(x)
#endif


/* This function dumps backtrace (or whatever similiar it founds on the stack)
 * nicely formatted and with symbols resolved to @f. When @trouble is set, it
 * tells it to be extremely careful and not use dynamic memory allocation
 * functions etc (useful in SIGSEGV handler etc). */
/* Note that this function just calls system-specific backend provided by the
 * libc, so it is available only on some systems. BACKTRACE is defined if it
 * is available on yours. */
#ifdef BACKTRACE
#include <stdio.h>
void dump_backtrace(FILE *f, int trouble);
#endif

#endif
