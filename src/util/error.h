/* $Id: error.h,v 1.5 2003/04/24 08:23:40 zas Exp $ */

#ifndef EL__UTIL_ERROR_H
#define EL__UTIL_ERROR_H

void _listmagicerror(unsigned char *, unsigned char *, int);

void force_dump();
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

#endif
