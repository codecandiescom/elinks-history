/* Error handling and debugging stuff */
/* $Id: error.c,v 1.14 2002/06/16 13:47:23 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "links.h"

#include "util/error.h"
#include "util/memlist.h"


#ifdef LEAK_DEBUG

#ifndef LEAK_DEBUG_LIST
struct alloc_header {
	int size;
};
#else
struct alloc_header {
	struct alloc_header *next;
	struct alloc_header *prev;
	int size;
	int line;
	unsigned char *file;
	unsigned char *comment;
};
#endif

#define SIZE_AH_ALIGNED ((sizeof(struct alloc_header) + 7) & ~7)

#endif


static inline void
force_dump()
{
	fprintf(stderr, "\n\033[1m%s\033[0m\n", "Forcing core dump");
	fflush(stderr);
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
	va_list params;

	va_start(params, fmt);

	sprintf(errbuf, "\033[1mINTERNAL ERROR\033[0m at %s:%d: ", errfile,
		errline);
	strcat(errbuf, fmt);
	er(1, errbuf, params);

	va_end(params);
	force_dump();
}

void
debug_msg(unsigned char *fmt, ...)
{
	unsigned char errbuf[4096];
	va_list params;

	va_start(params, fmt);

	sprintf(errbuf, "DEBUG MESSAGE at %s:%d: ", errfile, errline);
	strcat(errbuf, fmt);
	er(0, errbuf, params);

	va_end(params);
}



/* TODO: This should be probably in separate file. --pasky */

#ifdef LEAK_DEBUG

long mem_amount = 0;

#ifdef LEAK_DEBUG_LIST
struct list_head memory_list = { &memory_list, &memory_list };
#endif

void
check_memory_leaks()
{
	int comma = 0;
	struct alloc_header *ah;

	if (!mem_amount) {
		/* No leaks - escape now. */
		return;
	}

	fprintf(stderr, "\n\033[1mMemory leak by %ld bytes\033[0m\n",
		mem_amount);

#ifdef LEAK_DEBUG_LIST
	fprintf(stderr, "\nList of blocks: ");
	foreach (ah, memory_list) {
		fprintf(stderr, "%s%p:%d @ %s:%d", comma ? ", ": "",
			(char *) ah + SIZE_AH_ALIGNED,
			ah->size, ah->file, ah->line);
		comma = 1;
		if (ah->comment) fprintf(stderr, ":\"%s\"", ah->comment);
	}
	fprintf(stderr, "\n");
#endif

	force_dump();
}


void *
debug_mem_alloc(unsigned char *file, int line, size_t size)
{
	struct alloc_header *ah;
	void *ptr;

	if (!size) return DUMMY;

	mem_amount += size;
	size += SIZE_AH_ALIGNED;

	ptr = malloc(size);
	if (!ptr) {
		error("ERROR: out of memory (malloc returned NULL)\n");
		return NULL;
	}

	ah = ptr;
	ptr = (char *) ptr + SIZE_AH_ALIGNED;

	ah->size = size - SIZE_AH_ALIGNED;
#ifdef LEAK_DEBUG_LIST
	ah->file = file;
	ah->line = line;
	ah->comment = NULL;

	add_to_list(memory_list, ah);
#endif

	return ptr;
}

void *
debug_mem_calloc(unsigned char *file, int line, size_t size, size_t eltsize)
{
	struct alloc_header *ah;
	void *ptr;

	if (!size) return DUMMY;

	/* FIXME: Unfortunately, we can't pass eltsize through to calloc()
	 * itself, because we add bloat like alloc_header to it, which is
	 * difficult to be measured in eltsize. Maybe we should round it up to
	 * next eltsize multiple, but would it be worth the possibly wasted
	 * space? Someone should make some benchmarks. If you still read this
	 * comment, it means YOU should help us and do the benchmarks! :)
	 * Thanks a lot. --pasky */

	mem_amount += size * eltsize;
	size += SIZE_AH_ALIGNED;

	ptr = calloc(1, size);
	if (!ptr) {
		error("ERROR: out of memory (malloc returned NULL)\n");
		return NULL;
	}

	ah = ptr;
	ptr = (char *) ptr + SIZE_AH_ALIGNED;

	ah->size = size - SIZE_AH_ALIGNED;
#ifdef LEAK_DEBUG_LIST
	ah->file = file;
	ah->line = line;
	ah->comment = NULL;

	add_to_list(memory_list, ah);
#endif

	return ptr;
}

void
debug_mem_free(unsigned char *file, int line, void *ptr)
{
	struct alloc_header *ah;

	if (ptr == DUMMY) return;
	if (!ptr) {
		errfile = file;
		errline = line;
		int_error("mem_free(NULL)");
		return;
	}

	ptr = (char *) ptr - SIZE_AH_ALIGNED;
	ah = ptr;

#ifdef LEAK_DEBUG_LIST
	del_from_list(ah);
	if (ah->comment)
		free(ah->comment);
#endif

	mem_amount -= ah->size;
	free(ptr);
}

void *
debug_mem_realloc(unsigned char *file, int line, void *ptr, size_t size)
{
	struct alloc_header *ah;

	if (ptr == DUMMY) return debug_mem_alloc(file, line, size);
	if (!ptr) {
		errfile = file;
		errline = line;
		int_error("mem_realloc(NULL, %d)", size);
		return NULL;
	}

	if (!size) {
		debug_mem_free(file, line, ptr);
		return DUMMY;
	}

	ptr = realloc((char *) ptr - SIZE_AH_ALIGNED, size + SIZE_AH_ALIGNED);
	if (!ptr) {
		error("ERROR: out of memory (realloc returned NULL)\n");
		return NULL;
	}

	ah = ptr;
	mem_amount += size - ah->size;

	ah->size = size;
#ifdef LEAK_DEBUG_LIST
	ah->prev->next = ah;
	ah->next->prev = ah;
#endif

	return (char *) ptr + SIZE_AH_ALIGNED;
}

void
set_mem_comment(void *ptr, unsigned char *str, int len)
{
#ifdef LEAK_DEBUG_LIST
	struct alloc_header *ah;
	
	ah = (struct alloc_header *) ((char *) ptr - SIZE_AH_ALIGNED);

	if (ah->comment)
		free(ah->comment);

	ah->comment = malloc(len + 1);
	if (ah->comment)
		safe_strncpy(ah->comment, str, len + 1);
#endif
}

#endif

