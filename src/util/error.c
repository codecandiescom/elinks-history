/* Error handling and debugging stuff */
/* $Id: error.c,v 1.17 2002/06/16 16:22:41 zas Exp $ */

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

/* Fill memory on alloc() ?
 * Default is defined. */
#define FILL_ON_ALLOC
#define FILL_ON_ALLOC_VALUE 'X'

/* Fill memory on realloc() ?
 * Default is defined. */
#define FILL_ON_REALLOC
#define FILL_ON_REALLOC_VALUE 'Y'

/* Fill memory before free() ?
 * Default is undef. */
#undef FILL_ON_FREE
#define FILL_ON_FREE_VALUE 'Z'

/* Check alloc_header block sanity ?
 * Default is defined. */
#define CHECK_AH_SANITY
#define CHECK_AH_SANITY_MAGIC 0xD3BA110C

/* Check for realloc(NULL, size) ?
 * Default is undef. */
#undef CHECK_REALLOC_NULL

#ifndef LEAK_DEBUG_LIST
struct alloc_header {
#ifdef CHECK_AH_SANITY
	int magic;
#endif
	int size;
};
#else
struct alloc_header {
	struct alloc_header *next;
	struct alloc_header *prev;
#ifdef CHECK_AH_SANITY
	int magic;
#endif
	int size;
	int line;
	unsigned char *file;
	unsigned char *comment;
};
#endif

/* Size is set to be a multiple of 16, forcing aligment by the way. */
#define SIZE_AH_ALIGNED ((sizeof(struct alloc_header) + 15) & ~15)

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

#ifdef CHECK_AH_SANITY
int
bad_ah_sanity(struct alloc_header *ah, unsigned char *comment)
{
	if (!ah) return 1;
	if (ah->magic != CHECK_AH_SANITY_MAGIC) {
		if (comment && *comment) fprintf(stderr, "%s ", comment);
		fprintf(stderr, "%p:%d @ %s:%d magic:%08x != %08x @ %p",
				(char *) ah + SIZE_AH_ALIGNED,
				ah->size, ah->file, ah->line, ah->magic,
				CHECK_AH_SANITY_MAGIC, ah);
		return 1;
	}

	return 0;
}
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
#ifdef CHECK_AH_SANITY
		if (bad_ah_sanity(ah, "Skipped")) continue;
#endif
		fprintf(stderr, "%s%p:%d @ %s:%d", comma ? ", ": "",
			(char *) ah + SIZE_AH_ALIGNED,
			ah->size, ah->file, ah->line);
		comma = 1;
		if (ah->comment)
			fprintf(stderr, ":\"%s\"", ah->comment);
	}
	fprintf(stderr, "\n");
#endif

	force_dump();
}


void *
debug_mem_alloc(unsigned char *file, int line, size_t size)
{
	struct alloc_header *ah;

	if (!size) return DUMMY;

	ah = malloc(size + SIZE_AH_ALIGNED);
	if (!ah) {
		error("ERROR: out of memory (malloc returned NULL)\n");
		return NULL;
	}

#ifdef FILL_ON_ALLOC
	memset(ah, FILL_ON_ALLOC_VALUE, size + SIZE_AH_ALIGNED);
#endif

	mem_amount += size;

	ah->size = size;
#ifdef CHECK_AH_SANITY
	ah->magic = CHECK_AH_SANITY_MAGIC;
#endif
#ifdef LEAK_DEBUG_LIST
	ah->file = file;
	ah->line = line;
	ah->comment = NULL;

	add_to_list(memory_list, ah);
#endif

	return (void *) ((char *) ah + SIZE_AH_ALIGNED);
}

void *
debug_mem_calloc(unsigned char *file, int line, size_t eltcount, size_t eltsize)
{
	struct alloc_header *ah;
	size_t size = eltcount * eltsize;

	if (!size) return DUMMY;

	/* FIXME: Unfortunately, we can't pass eltsize through to calloc()
	 * itself, because we add bloat like alloc_header to it, which is
	 * difficult to be measured in eltsize. Maybe we should round it up to
	 * next eltsize multiple, but would it be worth the possibly wasted
	 * space? Someone should make some benchmarks. If you still read this
	 * comment, it means YOU should help us and do the benchmarks! :)
	 * Thanks a lot. --pasky */

	ah = calloc(1, size + SIZE_AH_ALIGNED);
	if (!ah) {
		error("ERROR: out of memory (malloc returned NULL)\n");
		return NULL;
	}

	mem_amount += size;

	ah->size = size;
#ifdef CHECK_AH_SANITY
	ah->magic = CHECK_AH_SANITY_MAGIC;
#endif
#ifdef LEAK_DEBUG_LIST
	ah->file = file;
	ah->line = line;
	ah->comment = NULL;

	add_to_list(memory_list, ah);
#endif

	return (void *) ((char *) ah + SIZE_AH_ALIGNED);
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

	ah = (struct alloc_header *) ((char *) ptr - SIZE_AH_ALIGNED);

#ifdef CHECK_AH_SANITY
	if (bad_ah_sanity(ah, "free()")) force_dump();
#endif

#ifdef LEAK_DEBUG_LIST
	if (ah->comment)
		free(ah->comment);
	del_from_list(ah);
#endif

	mem_amount -= ah->size;

#ifdef FILL_ON_FREE
	memset(ah, FILL_ON_FREE_VALUE, ah->size + SIZE_AH_ALIGNED);
#endif

	free(ah);
}

void *
debug_mem_realloc(unsigned char *file, int line, void *ptr, size_t size)
{
	struct alloc_header *ah;

#ifdef CHECK_REALLOC_NULL
	/* Disabled by default since glibc realloc() behaves like malloc(size)
	 * when passed pointer is NULL. */
	if (!ptr) {
		errfile = file;
		errline = line;
		int_error("mem_realloc(NULL, %d)", size);
		return NULL;
	}
#endif

	if (!ptr || ptr == DUMMY) return debug_mem_alloc(file, line, size);

	/* Frees memory if size is zero. */
	if (!size) {
		debug_mem_free(file, line, ptr);
		return DUMMY;
	}

	ah = (struct alloc_header *) ((char *) ptr - SIZE_AH_ALIGNED);

#ifdef CHECK_AH_SANITY
	if (bad_ah_sanity(ah, "realloc()")) force_dump();
#endif

	/* We compare oldsize to new size, and if equal we just return ptr
	 * and change nothing, this is conform to most realloc() behavior. */
	if (ah->size == size) return (void *) ptr;

	ah = realloc(ah, size + SIZE_AH_ALIGNED);
	if (!ah) {
		error("ERROR: out of memory (realloc returned NULL)\n");
		return NULL;
	}

	mem_amount += size - ah->size;

#ifdef FILL_ON_REALLOC
	if (size > ah->size)
		memset((char *) ah + SIZE_AH_ALIGNED + ah->size,
		       FILL_ON_REALLOC_VALUE, size - ah->size);
#endif

	ah->size = size;
#ifdef CHECK_AH_SANITY
	ah->magic = CHECK_AH_SANITY_MAGIC;
#endif
#ifdef LEAK_DEBUG_LIST
	ah->prev->next = ah;
	ah->next->prev = ah;
#endif

	return (void *) ((char *) ah + SIZE_AH_ALIGNED);
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

#undef FILL_ON_ALLOC
#undef FILL_ON_ALLOC_VALUE

#undef FILL_ON_REALLOC
#undef FILL_ON_REALLOC_VALUE

#undef FILL_ON_FREE
#undef FILL_ON_FREE_VALUE

#undef CHECK_REALLOC_NULL

#undef CHECK_AH_SANITY
#undef CHECK_AH_SANITY_MAGIC

#endif

