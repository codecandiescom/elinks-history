/* $Id: math.h,v 1.4 2004/12/20 23:51:25 pasky Exp $ */

#ifndef EL__UTIL_MATH_H
#define EL__UTIL_MATH_H


/* It's evil to include this directly, elinks.h includes it for you
 * at the right time. */


/* These macros will evaluate twice their arguments.
 * Ie. MIN(a+b, c+d) will do 3 additions...
 * Please prefer to use int_min() and int_max() if possible. */

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>	/* MIN/MAX may be defined in this header. */
#endif

/* FreeBSD needs this. */
#ifdef MIN
#undef MIN
#endif

#ifdef MAX
#undef MAX
#endif

#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))


static inline int
int_min(register int x, register int y)
{
	if (x < y) return x;
	return y;
}

static inline int
int_max(register int x, register int y)
{
	if (x > y) return x;
	return y;
}


/* Limit @what pointed value to upper bound @limit. */
static inline void
int_upper_bound(register int *what, register int limit)
{
	if (*what > limit) *what = limit;
}

/* Limit @what pointed value to lower bound @limit. */
static inline void
int_lower_bound(register int *what, register int limit)
{
	if (*what < limit) *what = limit;
}

/* Limit @what pointed value to lower bound @lower_limit and to upper bound
 * @upper_limit. */
static inline void
int_bounds(register int *what, register int lower_limit,
	   register int upper_limit)
{
	if (*what < lower_limit)
		*what = lower_limit;
	else if (*what > upper_limit)
		*what = upper_limit;
}


/* Give us the best you can. */
/* longlong is not defined now and we have to be careful about #includes
 * since we are in every .c file. math.c ensures this and longlong
 * stays in sync. */
extern
#ifdef HAVE_LONG_LONG
long long
#else
long
#endif
swap_register_;

#define swap_values(a,b) do { \
	/* These are supposed to evaluate at the compile time, giving us
	 * no performance hit. */ \
	assert(sizeof(a) == sizeof(b)); \
	assert(sizeof(a) <= sizeof(longlong)); \
	swap_register_ = (longlong) a; \
	a = b; \
	b = (longlong) swap_register_; \
} while (0)

#endif
