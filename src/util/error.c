/* Error handling and debugging stuff */
/* $Id: error.c,v 1.46 2003/05/07 15:16:30 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
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
#include "util/string.h"

void
list_magic_error_(unsigned char *where, unsigned char *what, unsigned char *file, int line)
{
	fprintf(stderr, "%s:%d", file, line);
	fprintf(stderr, " %s %s: bad list magic\n", where, what);
	fflush(stderr);
#ifdef LISTDEBUGFATAL
	raise(SIGSEGV);
#endif
}

void
force_dump()
{
	fprintf(stderr,
		"\n\033[1m%s\033[0m %s\n", "Forcing core dump!",
	        "Man the Lifeboats! Women and children first!\n");
	fflush(stderr);
	raise(SIGSEGV);
}

void
do_not_optimize_here(void *p)
{
	/* stop GCC optimization - avoid bugs in it */
}

void
er(int bell, unsigned char *fmt, va_list params)
{
	if (bell) fputc(7, stderr);
	vfprintf(stderr, fmt, params);
	fputc('\n', stderr);
	fflush(stderr);
}

void
error(unsigned char *fmt, ...)
{
	va_list params;

	va_start(params, fmt);
	er(1, fmt, params);
	va_end(params);
}

int errline;
unsigned char *errfile;

void
int_error(unsigned char *fmt, ...)
{
	unsigned char errbuf[4096];
	int size;
	int maxsize = sizeof(errbuf) - strlen(fmt);
	va_list params;

	va_start(params, fmt);

	size = snprintf(errbuf, maxsize, "\033[1mINTERNAL ERROR\033[0m at %s:%d: ",
			errfile, errline);
	if (size < maxsize) {
		strcat(errbuf, fmt);
		er(1, errbuf, params);
	}

	va_end(params);
#ifdef DEBUG
	force_dump();
#endif
}

void
debug_msg(unsigned char *fmt, ...)
{
	unsigned char errbuf[4096];
	int size;
	int maxsize = sizeof(errbuf) - strlen(fmt);
	va_list params;

	va_start(params, fmt);

	size = snprintf(errbuf, maxsize, "DEBUG MESSAGE at %s:%d: ",
			errfile, errline);
	if (size < maxsize) {
		strcat(errbuf, fmt);
		er(0, errbuf, params);
	}

	va_end(params);
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

	fprintf(f, "Wheeeeeeeeeee! You played with the config.h by hand, didn't you?\n");
	fprintf(f, "Of _COURSE_ you did! Is that how a nice .. creature behaves like?\n");
	fprintf(f, "Of _COURSE_ it isn't! I feel offended and thus I will revenge now!\n");
	fprintf(f, "You will _suffer_ >:).\n");
	fprintf(f, "\n");
	fprintf(f, "CPU burning sequence initiated...\n");
	/* TODO: Include cpuburn.c here. --pasky */
	while (1);
#endif
}

#endif
