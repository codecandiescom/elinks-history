/* Error handling and debugging stuff */
/* $Id: error.c,v 1.33 2002/11/26 22:47:01 pasky Exp $ */

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

#include "links.h"

#include "util/error.h"
#include "util/lists.h"
#include "util/memlist.h"
#include "util/memory.h"
#include "util/string.h"


void
force_dump()
{
	fprintf(stderr, "\n\033[1m%s\033[0m\n", "Forcing core dump");
	fprintf(stderr, "Man the Lifeboats! Women and children first!\n");
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
	if (bell) fprintf(stderr, "%c", (char)7);
	vfprintf(stderr, fmt, params);
	fprintf(stderr, "\n");
	fflush(stderr);
	sleep(1);
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
	force_dump();
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
