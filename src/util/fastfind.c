/* Very fast search_keyword_in_list.
 * It replaces bsearch() + strcasecmp() + callback + ...
 *
 * Following conditions should be met:
 *
 * - list keys are C strings.
 * - keys should not be greater than 255 characters, and optimally < 20
 *   characters. It can work with greater keys but then memory usage will
 *   grow a lot.
 * - each key must be unique and non empty.
 * - list do not have to be ordered.
 * - total number of unique characters used in all keys should be <= 128
 * - idealy total number of keys should be <= 512 (but see below)
 *
 *  (c) 2003 Laurent MONIN (aka Zas)
 * Feel free to do whatever you want with that code. */
/* $Id: fastfind.c,v 1.4 2003/06/13 18:38:23 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include "elinks.h"

#include "util/conv.h"
#include "util/fastfind.h"
#include "util/memdebug.h"
#include "util/memory.h"


#if 0
/* Use only 32 bits per element, but has very low limits. */
struct ff_elt {
	unsigned int e:1; /* End leaf -> p is significant */
	unsigned int c:1; /* Compressed */
	unsigned int p:9; /* Index in pointers (max 512) */
	unsigned int l:14; /* Index in lines (max 16384 - 1) */
	unsigned int ch:7; /* no use in elt */
};

struct ff_elt_c {
	unsigned int e:1; /* End leaf -> p is significant */
	unsigned int c:1; /* Compressed */
	unsigned int p:9; /* Index in pointers */
	unsigned int l:14; /* Index in lines */
	unsigned int ch:7; /* char when compressed. */
};
#else
/* Keep this one if there is more than 512 keywords in a list
 * it eats a bit more memory.
 * ELinks needs this one. */
struct ff_elt {
	unsigned int e:1; /* End leaf -> p is significant */
	unsigned int c:1; /* Compressed */
	unsigned int p:12; /* Index in pointers (4096) */
	unsigned int l:18; /* Index in lines (262144 - 1) */
};

struct ff_elt_c {
	unsigned int e:1; /* End leaf -> p is significant */
	unsigned int c:1; /* Compressed */
	unsigned int p:12; /* Index in pointers */
	unsigned int l:18; /* Index in lines */
	unsigned char ch;
};
#endif

struct fastfind_info {
	void **pointers;
	int *keylen_list;

	struct ff_elt **lines;
	struct ff_elt *root_line;

	int uniq_chars_count;
	int min_key_len;
	int max_key_len;
	int count;
	int case_sensitive;
	int idxtab[128];

	unsigned short pointers_count;
	unsigned short lines_count;

	unsigned char uniq_chars[128];
#ifdef FASTFIND_DEBUG
	unsigned long searches;
	unsigned long found;
	unsigned long iterations;
	unsigned long tests;
	unsigned long memory_usage;
	unsigned long total_key_len;
#endif
};


static struct fastfind_info *
ff_init(int case_sensitive)
{
	struct fastfind_info *info = mem_calloc(1, sizeof(struct fastfind_info));

	if (info) {
#ifdef FASTFIND_DEBUG
		info->memory_usage += sizeof(struct fastfind_info) - sizeof(unsigned long) * 6;
#endif
		info->min_key_len = 255; /* Non sense to use that code if key length > 255 so... */
		info->case_sensitive = !!case_sensitive;
	}

	return info;
}


/* Return 1 on success, 0 on allocation failure */
static int
add_to_pointers(void *p, int key_len, struct fastfind_info *info)
{
	/* FIXME: Check limit */
	info->pointers = mem_realloc(info->pointers, sizeof(void *) * (info->pointers_count + 1));
	if (!info->pointers) return 0; /* Freed at cleanup time */
#ifdef FASTFIND_DEBUG
	info->memory_usage += sizeof(void *);
#endif

	info->keylen_list = mem_realloc(info->keylen_list, sizeof(int) * (info->pointers_count + 1));
	if (!info->keylen_list) return 0; /* Freed at cleanup time */
#ifdef FASTFIND_DEBUG
	info->memory_usage += sizeof(int);
#endif
	info->pointers[info->pointers_count] = p;
	info->keylen_list[info->pointers_count] = key_len; /* Record key len, used in search */

	info->pointers_count++;

	return 1;
}

/* Return 1 on success, 0 on allocation failure */
static int
alloc_line(struct fastfind_info *info)
{
	/* FIXME: Check limit */
	struct ff_elt *line;

	info->lines = mem_realloc(info->lines, sizeof(struct ff_elt *) * (info->lines_count + 2));
	if (!info->lines) return 0;
#ifdef FASTFIND_DEBUG
	info->memory_usage += sizeof(struct ff_elt *);
#endif

	line = mem_calloc(sizeof(struct ff_elt), info->uniq_chars_count);
	if (!line) return 0;
#ifdef FASTFIND_DEBUG
	info->memory_usage += sizeof(struct ff_elt) * info->uniq_chars_count;
#endif
	/* info->lines[0] is never used since l=0 marks no leaf in struct ff_elt */
	info->lines_count++;
	info->lines[info->lines_count] = line;

	return 1;
}

static int
char2idx(unsigned char c, struct fastfind_info *info)
{
	int j;
	int idx = -1;

	for (j = 0; j < info->uniq_chars_count; j++)
		if (info->uniq_chars[j] == c) {
			idx = j;
			break;
		}

	return idx;
}

static void
init_idxtab(struct fastfind_info *info)
{
	int i;

	for (i = 0; i < 128; i++)
		info->idxtab[i] = char2idx((unsigned char) i, info);
}

/* This function must be called once and only once per list. --Zas */
void *
fastfind_index(void (*reset) (void), struct fastfind_key_value * (*next) (void), int case_sensitive)
{
	struct fastfind_key_value *p;
	struct fastfind_info *info = ff_init(case_sensitive);
	register int i;

	if (!info) return NULL;

	/* First search min, max, count */
	reset();
	while ((p = (*next)())) {
		int key_len = strlen(p->key);

		if (key_len < info->min_key_len)
			info->min_key_len = key_len;

		if (key_len > info->max_key_len)
			info->max_key_len = key_len;

		for (i = 0; i < key_len; i++) {
			int found = 0;
			int j;

			for (j = 0; j < info->uniq_chars_count; j++) {
				if (info->case_sensitive) {
					if (info->uniq_chars[j] == p->key[i]) {
						found = 1;
						break;
					}
				} else {
					if (info->uniq_chars[j] == upcase(p->key[i])) {
						found = 1;
						break;
					}
				}
			}
			/* FIXME: limit 128 */
			if (!found) {
				if (info->case_sensitive)
					info->uniq_chars[info->uniq_chars_count++] = p->key[i];
				else
					info->uniq_chars[info->uniq_chars_count++] = upcase(p->key[i]);
			}


		}
		info->count++;
	}

	if (!info->count) return 0;
#ifdef FASTFIND_DEBUG
	fprintf(stderr, "uniq_chars: %s\n", info->uniq_chars);
#endif
	init_idxtab(info);

	/* Root line allocation */
	if (!alloc_line(info)) {
		fastfind_terminate(info);
		return NULL;
	}

	info->root_line = info->lines[info->lines_count];

	/* Do it */
	(*reset)();
	while ((p = (*next)())) {
		int key_len = strlen(p->key);
		struct ff_elt *current = info->root_line;

#ifdef FASTFIND_DEBUG
		fprintf(stderr, "K: %s\n", p->key);
#endif
		for (i = 0; i < key_len; i++) {
			int idx;

			/* Convert char to its index value */
			if (info->case_sensitive)
				idx = info->idxtab[p->key[i]]; /* Convert char to its index value */
			else
				idx = info->idxtab[upcase(p->key[i])];

			if (key_len == i + 1) { /* End of the Word */
				current[idx].e = 1; /* Mark final leaf */
				/* Memorize pointer to data */
				current[idx].p = info->pointers_count;
				if (!add_to_pointers(p->data, key_len, info)) {
					fastfind_terminate(info);
					return NULL;
				}

			} else {
				if (current[idx].l == 0) { /* There's no leaf line yet */
					if (!alloc_line(info)) {
						fastfind_terminate(info);
						return NULL;
					}

					current[idx].l = info->lines_count;
				}

				current = info->lines[current[idx].l]; /* Descend to leaf line */
			}
		}
	}

	return (void *) info;
}

/* This one should be called to minimize memory usage of index.
 * Highly recommended but optionnal. */
void
fastfind_index_compress(void *current_, void *fastfind_info)
{
	register int i;
	struct ff_elt *current = (struct ff_elt *)current_;
	struct fastfind_info *info = (struct fastfind_info *) fastfind_info;
	int cnt = 0;
	int pos = -1;

	if (!info) return;

	if (!current) current = info->root_line;

	for (i = 0; i < info->uniq_chars_count; i++) {
		if (current[i].c) continue;

		if (current[i].l) /* There's a leaf line, descend to it, and recurse */
			fastfind_index_compress(info->lines[current[i].l], info);

		if (current[i].l || current[i].e) {
			cnt++;
			pos = i;
		}
	}

	/* Compress if possible ;) */
	if (pos != -1 && cnt < 2 && !current[pos].c) {
		int done = 0;
		struct ff_elt_c *new = mem_alloc(sizeof(struct ff_elt_c));

#ifdef FASTFIND_DEBUG
		info->memory_usage += sizeof(struct ff_elt_c);
#endif

		new->c = 1;
		new->e = current[pos].e;
		new->p = current[pos].p;
		new->l = current[pos].l;
		new->ch = pos;

		for (i = 1; i < info->lines_count; i++) {
			if (info->lines[i] == current) {
				mem_free(info->lines[i]);
				info->lines[i] = (struct ff_elt *) new;
#ifdef FASTFIND_DEBUG
			/*	fprintf(stderr, "comp: %p %d\n", current, i); */
				info->memory_usage -= sizeof(struct ff_elt) * info->uniq_chars_count;
#endif
				done = 1;
				break;
			}
		}
		if (!done) mem_free(new);
	}
}

/* The main reason of all that stuff is here. --Zas */
void *
fastfind_search(unsigned char *key, int key_len, void *fastfind_info)
{
	struct fastfind_info *info = (struct fastfind_info *) fastfind_info;
	register int i;
	struct ff_elt *current;

#ifdef FASTFIND_DEBUG
	info->searches++;
	info->total_key_len += key_len;
	info->tests++;
   	if (!info) return NULL;
	info->tests++;
   	if (!key) return NULL;
	info->tests++;
	if (key_len > info->max_key_len) return NULL;
	info->tests++;
	if (key_len < info->min_key_len) return NULL;
#endif

	if (!info || !key || key_len > info->max_key_len || key_len < info->min_key_len)
		return NULL;

	current = info->root_line;

	for (i = 0; i < key_len; i++) {
		int lidx;

#ifdef FASTFIND_DEBUG
		info->tests++;
#endif
		if (info->case_sensitive)
			lidx = info->idxtab[key[i]];
		else
			lidx = info->idxtab[upcase(key[i])];

#ifdef FASTFIND_DEBUG
		info->iterations++;
		info->tests++;
#endif
		if (lidx < 0) return NULL;

#ifdef FASTFIND_DEBUG
		info->tests++;
#endif
		if (current->c) {
#ifdef FASTFIND_DEBUG
			info->tests++;
#endif
			if (((struct ff_elt_c *)(current))->ch != lidx)
				return NULL;
#ifdef FASTFIND_DEBUG
			info->tests++;
#endif
			if (current->e && key_len == info->keylen_list[current->p]) {
#ifdef FASTFIND_DEBUG
				info->found++;
				info->tests++;
#endif
				return info->pointers[current->p];
			}
#ifdef FASTFIND_DEBUG
			info->tests++;
#endif
			if (current->l)
				current = (struct ff_elt *) info->lines[current->l];
			else
				return NULL;

		} else {
#ifdef FASTFIND_DEBUG
			info->tests++;
#endif
			if (current[lidx].e && key_len == info->keylen_list[current[lidx].p]) {
#ifdef FASTFIND_DEBUG
				info->found++;
				info->tests++;
#endif
				return info->pointers[current[lidx].p];
			}
#ifdef FASTFIND_DEBUG
			info->tests++;
#endif
			if (current[lidx].l)
				current = (struct ff_elt *) info->lines[current[lidx].l];
			else
				return NULL;

		}
	}

	return NULL;
}

/* Fastfind cleanup, it frees index, must be called once per list. --Zas */
void
fastfind_terminate(void *fastfind_info)
{
	struct fastfind_info *info = (struct fastfind_info *) fastfind_info;

	if (!info) return;
#ifdef FASTFIND_DEBUG
	fprintf(stderr, "Entries     : %d\n", info->pointers_count);
	fprintf(stderr, "Memory usage: %lu bytes (cost per entry = %0.2f bytes)\n", info->memory_usage, (double) info->memory_usage / info->pointers_count);
	fprintf(stderr, "Searches    : %lu\n", info->searches);
	fprintf(stderr, "Found       : %lu (%0.2f%%)\n", info->found, 100 * (double) info->found / info->searches);
	fprintf(stderr, "Iterations  : %lu (%0.2f per search)\n", info->iterations, (double) info->iterations / info->searches);
	fprintf(stderr, "Tests       : %lu (%0.2f per search)\n", info->tests, (double) info->tests / info->searches);
	fprintf(stderr, "Total keylen: %lu bytes (%0.2f per search)\n", info->total_key_len, (double) info->total_key_len / info->searches);
#endif

	if (info->pointers) mem_free(info->pointers);
	if (info->keylen_list) mem_free(info->keylen_list);
	while (info->lines_count) {
		if (info->lines[info->lines_count])
			mem_free(info->lines[info->lines_count]);
		info->lines_count--;
	}
	if (info->lines) mem_free(info->lines);
	mem_free(info);
}


/* EXAMPLE */

#if 0
struct list {
	unsigned char *tag;
	int val;
};

struct list list[] = {
	{"A",		1},
	{"ABBR",	2},
	{"ADDRESS",	3},
	{"B",		4},
	{"BASE",	5},
	{"BASEFONT",	6},
	{"BLOCKQUOTE",	7},
	{"BODY",	8},
	{"BR",		9},
	{"BUTTON",	10},
	{"CAPTION",	11},
	{"CENTER",	12},
	{"CODE",	13},
	{"DD",		14},
	{"DFN",		15},
	{"DIR",		16},
	{"DIV",		17},
	{"DL",		18},
	{"DT",		19},
	{"EM",		20},
	{"FIXED",	21},
	{"FONT",	22},
	{"FORM",	23},
	{"FRAME",	24},
	{"FRAMESET",	25},
	{"H1",		26},
	{"H2",		27},
	{"H3",		28},
	{"H4",		29},
	{"H5",		30},
	{"H6",		31},
	/* {"HEAD",	html_skip,	0, 0}, */
	{"HR",		32},
	{"I",		33},
	{"IFRAME",	34},
	{"IMG",		35},
	{"INPUT",	36},
	{"LI",		37},
	{"LINK",	38},
	{"LISTING",	39},
	{"MENU",	40},
	{"NOFRAMES",	41},
	{"OL",		42},
	{"OPTION",	43},
	{"P",		44},
	{"PRE",		45},
	{"Q",		46},
	{"S",		47},
	{"SCRIPT",	48},
	{"SELECT",	49},
	{"SPAN",	50},
	{"STRIKE",	51},
	{"STRONG",	52},
	{"STYLE",	53},
	{"SUB",		54},
	{"SUP",		55},
	{"TABLE",	56},
	{"TD",		57},
	{"TEXTAREA",	58},
	{"TH",		59},
	{"TITLE",	60},
	{"TR",		61},
	{"U",		62},
	{"UL",		63},
	{"XMP",		64},
	{NULL,		0}, /* List terminaison is key = NULL */
};


struct list *internal_pointer;

/* Reset internal list pointer */
void
reset_list(void)
{
	internal_pointer = list;
}

/* Returns a pointer to a struct that contains
 * current key and data pointers and increment
 * internal pointer.
 * It returns NULL when key is NULL. */
struct fastfind_key_value *
next_in_list(void)
{
	static struct fastfind_key_value kv;

	if (!internal_pointer->tag) return NULL;

	kv.key = internal_pointer->tag;
	kv.data = internal_pointer;

	internal_pointer++;

	return &kv;
}


int
main(int argc, char **argv)
{
	unsigned char *key = argv[1];
	struct list *result;
	void *info;

	if (!key || !*key) {
		fprintf(stderr, "Usage: fastfind keyword\n");
		exit(-1);
	}

	fprintf(stderr, "---------- INDEX PHASE -----------\n");
	/* Mandatory */
	info = fastfind_index(&reset_list, &next_in_list);

	fprintf(stderr, "--------- COMPRESS PHASE ---------\n");
	/* Highly recommended but optionnal. */
	fastfind_index_compress(NULL, info);

	fprintf(stderr, "---------- SEARCH PHASE ----------\n");
	/* Without this one ... */
	result = (struct list *) fastfind_search(key, strlen(key), info);

	if (result)
		fprintf(stderr, " Found: '%s' -> %d\n", result->tag, result->val);
	else
		fprintf(stderr, " Not found: '%s'\n", key);

	fastfind_terminate(info);

	exit(0);
}

#endif
