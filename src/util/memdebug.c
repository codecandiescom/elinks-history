/* Memory debugging (leaks, overflows & co) */
/* $Id: memdebug.c,v 1.1 2002/06/17 11:23:46 pasky Exp $ */

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
#include "util/memdebug.h"
#include "util/memory.h"
#include "util/string.h"


#ifdef LEAK_DEBUG

/* Eat less memory, but sacrifice speed?
 * Default is defined. */
#define LESS_MEMORY_SPEED

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
#define AH_SANITY_MAGIC 0xD3BA110C

/* Check for realloc(NULL, size) ?
 * Default is undef. */
#undef CHECK_REALLOC_NULL


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

/* Size is set to be on boundary of 8 (a multiple of 7) in order to have the
 * main ptr aligned properly (faster access). We hope that  */
#ifdef LESS_MEMORY_SPEED
#define SIZE_AH_ALIGNED ((sizeof(struct alloc_header) + 7) & ~7)
#else
/* Especially on 128bit machines, this can be faster, but eats more memory. */
#define SIZE_AH_ALIGNED ((sizeof(struct alloc_header) + 15) & ~15)
#endif

/* These macros are used to convert pointers and sizes to or from real ones
 * when using alloc_header stuff. */
#define PTR_AH2BASE(ah) (void *) ((char *) (ah) + SIZE_AH_ALIGNED)
#define PTR_BASE2AH(ptr) (struct alloc_header *) \
				((char *) (ptr) - SIZE_AH_ALIGNED)

#define SIZE_BASE2AH(size) ((size) + SIZE_AH_ALIGNED)
#define SIZE_AH2BASE(size) ((size) - SIZE_AH_ALIGNED)


long mem_amount = 0;

struct list_head memory_list = { &memory_list, &memory_list };

#ifdef CHECK_AH_SANITY
static int
bad_ah_sanity(struct alloc_header *ah, unsigned char *comment)
{
	if (!ah) return 1;
	if (ah->magic != AH_SANITY_MAGIC) {
		if (comment && *comment) fprintf(stderr, "%s ", comment);
		fprintf(stderr, "%p:%d @ %s:%d magic:%08x != %08x @ %p",
				PTR_AH2BASE(ah),
				ah->size, ah->file, ah->line, ah->magic,
				AH_SANITY_MAGIC, ah);
		return 1;
	}

	return 0;
}
#endif /* CHECK_AH_SANITY */

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

	fprintf(stderr, "\nList of blocks: ");
	foreach (ah, memory_list) {
#ifdef CHECK_AH_SANITY
		if (bad_ah_sanity(ah, "Skipped")) continue;
#endif
		fprintf(stderr, "%s%p:%d @ %s:%d", comma ? ", ": "",
			PTR_AH2BASE(ah),
			ah->size, ah->file, ah->line);
		comma = 1;
		if (ah->comment)
			fprintf(stderr, ":\"%s\"", ah->comment);
	}
	fprintf(stderr, "\n");

	force_dump();
}


void *
debug_mem_alloc(unsigned char *file, int line, size_t size)
{
	struct alloc_header *ah;

	if (!size) return DUMMY;

	ah = malloc(SIZE_BASE2AH(size));
	if (!ah) {
		error("ERROR: out of memory (malloc returned NULL)\n");
		return NULL;
	}

#ifdef FILL_ON_ALLOC
	memset(ah, FILL_ON_ALLOC_VALUE, SIZE_BASE2AH(size));
#endif

	mem_amount += size;

	ah->size = size;
#ifdef CHECK_AH_SANITY
	ah->magic = AH_SANITY_MAGIC;
#endif
	ah->file = file;
	ah->line = line;
	ah->comment = NULL;

	add_to_list(memory_list, ah);

	return PTR_AH2BASE(ah);
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

	ah = calloc(1, SIZE_BASE2AH(size));
	if (!ah) {
		error("ERROR: out of memory (malloc returned NULL)\n");
		return NULL;
	}

	/* No, we do NOT want to fill this with FILL_ON_ALLOC_VALUE ;)). */

	mem_amount += size;

	ah->size = size;
#ifdef CHECK_AH_SANITY
	ah->magic = AH_SANITY_MAGIC;
#endif
	ah->file = file;
	ah->line = line;
	ah->comment = NULL;

	add_to_list(memory_list, ah);

	return PTR_AH2BASE(ah);
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

	ah = PTR_BASE2AH(ptr);

#ifdef CHECK_AH_SANITY
	if (bad_ah_sanity(ah, "free()")) force_dump();
#endif

	if (ah->comment)
		free(ah->comment);
	del_from_list(ah);

	mem_amount -= ah->size;

#ifdef FILL_ON_FREE
	memset(ah, FILL_ON_FREE_VALUE, SIZE_BASE2AH(ah->size));
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

	ah = PTR_BASE2AH(ptr);

#ifdef CHECK_AH_SANITY
	if (bad_ah_sanity(ah, "realloc()")) force_dump();
#endif

	/* We compare oldsize to new size, and if equal we just return ptr
	 * and change nothing, this conforms to usual realloc() behavior. */
	if (ah->size == size) return (void *) ptr;

	ah = realloc(ah, SIZE_BASE2AH(size));
	if (!ah) {
		error("ERROR: out of memory (realloc returned NULL)\n");
		return NULL;
	}

	mem_amount += size - ah->size;

#ifdef FILL_ON_REALLOC
	if (size > ah->size)
		memset((char *) PTR_AH2BASE(ah) + ah->size,
		       FILL_ON_REALLOC_VALUE, size - ah->size);
#endif

	ah->size = size;
#ifdef CHECK_AH_SANITY
	ah->magic = AH_SANITY_MAGIC;
#endif
	ah->prev->next = ah;
	ah->next->prev = ah;

	return PTR_AH2BASE(ah);
}

void
set_mem_comment(void *ptr, unsigned char *str, int len)
{
	struct alloc_header *ah;

	ah = PTR_BASE2AH(ptr);

	if (ah->comment)
		free(ah->comment);

	ah->comment = malloc(len + 1);
	if (ah->comment)
		safe_strncpy(ah->comment, str, len + 1);
}

/* Remember, we undef stuff in the separate order that we define it. */

#undef SIZE_BASE2AH
#undef SIZE_AH2BASE

#undef PTR_AH2BASE
#undef PTR_BASE2AH

#undef CHECK_AH_SANITY
#undef AH_SANITY_MAGIC

#undef CHECK_REALLOC_NULL

#undef FILL_ON_FREE
#undef FILL_ON_FREE_VALUE

#undef FILL_ON_REALLOC
#undef FILL_ON_REALLOC_VALUE

#undef FILL_ON_ALLOC
#undef FILL_ON_ALLOC_VALUE

#endif /* LEAK_DEBUG */
