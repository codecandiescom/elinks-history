/* $Id: error.h,v 1.12 2003/05/08 21:50:09 zas Exp $ */

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

#ifdef BACKTRACE
#include <stdio.h>
void dump_backtrace(FILE *, int);
#endif

#endif
