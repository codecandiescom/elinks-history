/* Error handling and debugging stuff */
/* $Id: error.c,v 1.65 2003/06/17 07:17:27 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#ifndef _GNU_SOURCE
#define _GNU_SOURCE /* Needed for vasprintf() */
#endif

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "elinks.h"

#include "util/error.h"
#include "util/lists.h"
#include "util/memlist.h"
#include "util/memory.h"
#include "util/snprintf.h"
#include "util/string.h"
#include "util/version.h"


static void
er(int bell, unsigned char *fmt, va_list params)
{
	if (bell) fputc(7, stderr);
	vfprintf(stderr, fmt, params);
	fputc('\n', stderr);
	fflush(stderr);
}

int errline;
unsigned char *errfile;

void
elinks_debug(unsigned char *fmt, ...)
{
	unsigned char errbuf[4096];
	va_list params;

	va_start(params, fmt);

	snprintf(errbuf, sizeof(errbuf), "DEBUG MESSAGE at %s:%d: %s",
		 errfile, errline, fmt);

	er(0, errbuf, params);

	va_end(params);
}

void
elinks_error(unsigned char *fmt, ...)
{
	unsigned char errbuf[4096];
	va_list params;

	va_start(params, fmt);

	snprintf(errbuf, sizeof(errbuf), "ERROR at %s:%d: %s",
		 errfile, errline, fmt);

	er(1, errbuf, params);

	va_end(params);
}

void
elinks_internal(unsigned char *fmt, ...)
{
	unsigned char errbuf[4096];
	va_list params;

	va_start(params, fmt);

	snprintf(errbuf, sizeof(errbuf),
		 "\033[1mINTERNAL ERROR\033[0m at %s:%d: %s",
		 errfile, errline, fmt);

	er(1, errbuf, params);

	va_end(params);
#ifdef DEBUG
	force_dump();
#endif
}


void
elinks_assertm(int x, unsigned char *fmt, ...)
{
	unsigned char *buf;
	va_list params;

	if (x) return;

	va_start(params, fmt);
	vasprintf((char **) &buf, fmt, params);
	va_end(params);
	internal("assertion failed: %s", buf);
}


#ifdef DEBUG
void
force_dump(void)
{
	fprintf(stderr,
		"\n\033[1m%s\033[0m %s\n", "Forcing core dump!",
	        "Man the Lifeboats! Women and children first!\n");
	fputs(full_static_version, stderr);
	fputc('\n', stderr);
	fflush(stderr);
	raise(SIGSEGV);
}
#endif


void
do_not_optimize_here(void *p)
{
	/* stop GCC optimization - avoid bugs in it */
}


#ifdef BACKTRACE

/* The backtrace corner. */

#include <stdio.h>
#include <stdlib.h>

#ifdef HAVE_EXECINFO_H
#include <execinfo.h>
#endif

void
dump_backtrace(FILE *f, int trouble)
{
	/* If trouble is set, when we get here, we can be in various
	 * interesting situations like inside of the SIGSEGV handler etc. So be
	 * especially careful here.  Dynamic memory allocation may not work
	 * (corrupted stack). A lot of other things may not work too. So better
	 * don't do anything not 100% necessary. */

#ifdef HAVE_EXECINFO_H
	/* glibc way of doing this */

	void *stack[20];
	size_t size;
	char **strings;
	size_t i;

	size = backtrace(stack, 20);

	if (trouble) {
		/* Let's hope fileno() is safe. */
		backtrace_symbols_fd(stack, size, fileno(f));
		/* Now out! */
		return;
	}

	strings = backtrace_symbols(stack, size);

	fprintf(f, "Obtained %d stack frames:\n", size);

	for (i = 0; i < size; i++)
		fprintf(f, "[%p] %s\n", stack[i], strings[i]);

	free(strings);

#else
	/* user torturation */
	/* TODO: Gettextify? Er, better not. More people (translators) could
	 * find out what are we doing... ;-) --pasky */
	/* TODO: Be more cruel when in trouble? ;-) --pasky */

	fputs(	"Wheeeeeeeeeee! You played with the config.h by hand, didn't you?\n"
		"Of _COURSE_ you did! Is that how a nice .. creature behaves like?\n"
		"Of _COURSE_ it isn't! I feel offended and thus I will revenge now!\n"
		"You will _suffer_ >:).\n"
		"\n"
		"CPU burning sequence initiated...\n", f);

	/* TODO: Include cpuburn.c here. --pasky */
	while (1);
#endif
	fflush(f);
}

#endif
