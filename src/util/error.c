/* Error handling and debugging stuff */
/* $Id: error.c,v 1.5 2002/03/18 22:12:33 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <links.h>

#include <util/error.h>
#include <util/memlist.h>


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

#define L_D_S ((sizeof(struct alloc_header) + 7) & ~7)

#endif


#ifdef SPECIAL_MALLOC

/* Again, this is inherited from old links and I've no idea wtf is it ;). */

void *sp_malloc(size_t);
void sp_free(void *);
void *sp_realloc(void *, size_t);

#define xmalloc sp_malloc
#define xfree sp_free
#define xrealloc sp_realloc

#else

#define xmalloc malloc
#define xfree free
#define xrealloc realloc

#endif


static inline void force_dump()
{
	fprintf(stderr, "\n\033[1m%s\033[0m\n", "Forcing core dump");
	fflush(stderr);
	raise(SIGSEGV);
}


void do_not_optimize_here(void *p)
{
	/* stop GCC optimization - avoid bugs in it */
}

void er(int b, unsigned char *m, va_list l)
{
	if (b) fprintf(stderr, "%c", (char)7);
	vfprintf(stderr, m, l);
	fprintf(stderr, "\n");
	fflush(stderr);
	sleep(1);
}

void error(unsigned char *m, ...)
{
	va_list l;
	va_start(l, m);
	er(1, m, l);
}

int errline;
unsigned char *errfile;

unsigned char errbuf[4096];

void int_error(unsigned char *m, ...)
{
	va_list l;
	va_start(l, m);
	sprintf(errbuf, "\033[1mINTERNAL ERROR\033[0m at %s:%d: ", errfile, errline);
	strcat(errbuf, m);
	er(1, errbuf, l);
	force_dump();
}

void debug_msg(unsigned char *m, ...)
{
	va_list l;
	va_start(l, m);
	sprintf(errbuf, "DEBUG MESSAGE at %s:%d: ", errfile, errline);
	strcat(errbuf, m);
	er(0, errbuf, l);
}



/* TODO: This should be probably in separate file. --pasky */

#ifdef LEAK_DEBUG

long mem_amount = 0;

#ifdef LEAK_DEBUG_LIST
struct list_head memory_list = { &memory_list, &memory_list };
#endif

void check_memory_leaks()
{
	if (mem_amount) {
		fprintf(stderr, "\n\033[1mMemory leak by %ld bytes\033[0m\n", mem_amount);
#ifdef LEAK_DEBUG_LIST
		fprintf(stderr, "\nList of blocks: ");
		{
			int r = 0;
			struct alloc_header *ah;
			foreach (ah, memory_list) {
				fprintf(stderr, "%s%p:%d @ %s:%d", r ? ", ": "", (char *)ah + L_D_S, ah->size, ah->file, ah->line), r = 1;
				if (ah->comment) fprintf(stderr, ":\"%s\"", ah->comment);
			}
			fprintf(stderr, "\n");
		}
#endif
		force_dump();
	}
}


void *debug_mem_alloc(unsigned char *file, int line, size_t size)
{
	void *p;
	struct alloc_header *ah;

	if (!size) return DUMMY;

	mem_amount += size;
	size += L_D_S;

	if (!(p = xmalloc(size))) {
		error("ERROR: out of memory (malloc returned NULL)\n");
		return NULL;
	}

	ah = p;
	p = (char *)p + L_D_S;

	ah->size = size - L_D_S;
#ifdef LEAK_DEBUG_LIST
	ah->file = file;
	ah->line = line;
	ah->comment = NULL;

	add_to_list(memory_list, ah);
#endif

	return p;
}

void debug_mem_free(unsigned char *file, int line, void *p)
{
	struct alloc_header *ah;

	if (p == DUMMY) return;
	if (!p) {
		errfile = file, errline = line, int_error("mem_free(NULL)");
		return;
	}

	p = (char *)p - L_D_S;
	ah = p;
#ifdef LEAK_DEBUG_LIST
	del_from_list(ah);
	if (ah->comment) free(ah->comment);
#endif
	mem_amount -= ah->size;
	xfree(p);
}

void *debug_mem_realloc(unsigned char *file, int line, void *p, size_t size)
{
	struct alloc_header *ah;

	if (p == DUMMY) return debug_mem_alloc(file, line, size);
	if (!p) {
		errfile = file, errline = line, int_error("mem_realloc(NULL, %d)", size);
		return NULL;
	}
	if (!size) {
		debug_mem_free(file, line, p);
		return DUMMY;
	}
	if (!(p = xrealloc((char *)p - L_D_S, size + L_D_S))) {
		error("ERROR: out of memory (realloc returned NULL)\n");
		return NULL;
	}

	ah = p;
	mem_amount += size - ah->size;
	ah->size = size;
#ifdef LEAK_DEBUG_LIST
	ah->prev->next = ah;
	ah->next->prev = ah;
#endif
	return (char *)p + L_D_S;
}

void set_mem_comment(void *p, unsigned char *c, int l)
{
#ifdef LEAK_DEBUG_LIST
	struct alloc_header *ah = (struct alloc_header *)((char *)p - L_D_S);

	if (ah->comment) free(ah->comment);
	if ((ah->comment = malloc(l + 1))) memcpy(ah->comment, c, l), ah->comment[l] = 0;
#endif
}

#endif

