/* Very fast search_keyword_in_list. */
/* $Id: fastfind.c,v 1.62 2004/10/26 21:36:38 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>

#include "elinks.h"

#include "util/conv.h"
#include "util/error.h"
#include "util/fastfind.h"
#include "util/memdebug.h"
#include "util/memory.h"

#ifdef USE_FASTFIND

/* It replaces bsearch() + strcasecmp() + callback + ...
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


/* These routines use a tree search. First, a big tree is composed from the
 * keys on input. Then, when searching we just go through the tree. If we will
 * end up on an 'ending' node, we've got it.
 *
 * Hm, okay. For keys { 'head', 'h1', 'body', 'bodyrock', 'bodyground' }, it
 * would look like:
 *
 *             [root]
 *          b          h
 *          o        e   1
 *          d        a
 *          Y        D
 *        g   r
 *        r   o
 *        o   c
 *        u   K
 *        D
 *
 * (the ending nodes are upcased just for this drawing, not in real)
 *
 * To optimize this for speed, leafs of nodes are organized in per-node arrays
 * (so-called 'leafsets'), indexed by symbol value of the key's next character.
 * But to optimize that for memory, we first compose own alphabet consisting
 * only from the chars we ever use in the key strings. @uniq_chars holds that
 * alphabet and @idxtab is used to translate between it and ASCII.
 *
 * Tree building: O((L+M)*N)
 * 			(L: mean key length, M: alphabet size,
 * 			 N: number of items).
 * String lookup: O(N) (N: string length). */


/* Define it to generate performance and memory usage statistics to stderr. */
#if 0
#define DEBUG_FASTFIND
#endif

/* Define whether to use 32 or 64 bits per compressed element. */
#if 1
#define USE_32_BITS
#endif

#define END_LEAF_BITS		1
#define COMPRESSED_BITS		1

#ifdef USE_32_BITS

/* Use only 32 bits per element, but has very low limits. */
/* Adequate for ELinks tags search. */

#define POINTER_INDEX_BITS	10	/* 1024 */
#define LEAFSET_INDEX_BITS	13	/* 8192 */
#define COMP_CHAR_INDEX_BITS	7	/* 128	*/

#define ff_node ff_node_c /* Both are 32 bits long. */

#if (POINTER_INDEX_BITS + LEAFSET_INDEX_BITS + \
     COMP_CHAR_INDEX_BITS + END_LEAF_BITS + \
     COMPRESSED_BITS) > 32
#error Over 32 bits in struct ff_node !!
#endif

#else /* !USE_32_BITS */

/* Keep this one if there is more than 512 keywords in a list
 * it eats a bit more memory.
 * ELinks may need this one if fastfind is used in other
 * things than tags searching. */
/* This will make struct ff_node_c use 64 bits. */

#define POINTER_INDEX_BITS	12
#define LEAFSET_INDEX_BITS	18
#define COMP_CHAR_INDEX_BITS	8

#if (POINTER_INDEX_BITS + LEAFSET_INDEX_BITS + \
     + END_LEAF_BITS + COMPRESSED_BITS) > 32
#error Over 32 bits in struct ff_node !!
#endif

struct ff_node {
	/* End leaf -> p is significant */
	unsigned int e:END_LEAF_BITS;

	/* Compressed */
	unsigned int c:COMPRESSED_BITS;

	/* Index in pointers */
	unsigned int p:POINTER_INDEX_BITS;

	/* Index in leafsets */
	unsigned int l:LEAFSET_INDEX_BITS;
};

#endif /* USE_32_BITS */


#define FF_MAX_KEYS	(1  << POINTER_INDEX_BITS)
#define FF_MAX_LEAFSETS ((1 << LEAFSET_INDEX_BITS) - 1)
#define FF_MAX_CHARS	(1  << COMP_CHAR_INDEX_BITS)


struct ff_node_c {
	unsigned int e:END_LEAF_BITS;
	unsigned int c:COMPRESSED_BITS;
	unsigned int p:POINTER_INDEX_BITS;
	unsigned int l:LEAFSET_INDEX_BITS;

	/* Index of char when compressed. */
	unsigned int ch:COMP_CHAR_INDEX_BITS;
};


struct fastfind_info {
	void **pointers;
	int *keylen_list;

	struct ff_node **leafsets;
	struct ff_node *root_leafset;

	int uniq_chars_count;
	int min_key_len;
	int max_key_len;
	int count;
	int case_sensitive;
	int idxtab[FF_MAX_CHARS];

	int pointers_count;
	int leafsets_count;

#ifdef DEBUG_FASTFIND
	struct {
		unsigned long searches;
		unsigned long found;
		unsigned long itertmp;
		unsigned long iterdelta;
		unsigned long itermax;
		unsigned long iterations;
		unsigned long tests;
		unsigned long teststmp;
		unsigned long testsdelta;
		unsigned long testsmax;
		unsigned long memory_usage;
		unsigned long total_key_len;
		unsigned char *comment;
	} debug;
#endif

	unsigned char uniq_chars[FF_MAX_CHARS];
};


#ifdef DEBUG_FASTFIND
/* These are for performance testing. */
#define FF_DBG_mem(x, size) (x)->debug.memory_usage += (size)
#define FF_DBG_test(x) (x)->debug.tests++
#define FF_DBG_iter(x) (x)->debug.iterations++
#define FF_DBG_found(x) \
	do { \
		unsigned long iter = (x)->debug.iterations - (x)->debug.itertmp;	\
		unsigned long tests = (x)->debug.tests - (x)->debug.teststmp;		\
											\
		(x)->debug.iterdelta += iter;						\
		(x)->debug.testsdelta += tests;						\
		if (iter > (x)->debug.itermax)						\
			(x)->debug.itermax = iter;					\
		if (tests > (x)->debug.testsmax)					\
			(x)->debug.testsmax = tests;					\
		(x)->debug.found++;							\
	} while (0)
#define FF_DBG_comment(x, comment) do { (x)->debug.comment = empty_string_or_(comment); } while (0)

/* Update search stats. */
static void
FF_DBG_search_stats(struct fastfind_info *info, int key_len)
{
	info->debug.searches++;
	info->debug.total_key_len += key_len;
	info->debug.teststmp = info->debug.tests;
	info->debug.itertmp = info->debug.iterations;
}

/* Dump all stats. */
static void
FF_DBG_dump_stats(struct fastfind_info *info)
{
	fprintf(stderr, "------ FastFind Statistics ------\n");
	fprintf(stderr, "Comment     : %s\n", info->debug.comment);
	fprintf(stderr, "Case        : %s\n", info->case_sensitive ? "sensitive" : "insensitive");
	fprintf(stderr, "Uniq_chars  : %s\n", info->uniq_chars);
	fprintf(stderr, "Uniq_chars #: %d/%d max.\n", info->uniq_chars_count, FF_MAX_CHARS);
	fprintf(stderr, "Min_key_len : %d\n", info->min_key_len);
	fprintf(stderr, "Max_key_len : %d\n", info->max_key_len);
	fprintf(stderr, "Entries     : %d/%d max.\n", info->pointers_count, FF_MAX_KEYS);
	fprintf(stderr, "FFleafsets  : %d/%d max.\n", info->leafsets_count, FF_MAX_LEAFSETS);
	fprintf(stderr, "Memory usage: %lu bytes (cost per entry = %0.2f bytes)\n",
		info->debug.memory_usage, (double) info->debug.memory_usage / info->pointers_count);
	fprintf(stderr, "struct info : %d bytes\n", sizeof(struct fastfind_info) - sizeof(info->debug));
	fprintf(stderr, "Struct node : %d bytes (normal) , %d bytes (compressed)\n",
		sizeof(struct ff_node), sizeof(struct ff_node_c));
	fprintf(stderr, "Searches    : %lu\n", info->debug.searches);
	fprintf(stderr, "Found       : %lu (%0.2f%%)\n",
		info->debug.found, 100 * (double) info->debug.found / info->debug.searches);
	fprintf(stderr, "Iterations  : %lu (%0.2f per search, %0.2f before found, %lu max)\n",
		info->debug.iterations, (double) info->debug.iterations / info->debug.searches,
		(double) info->debug.iterdelta / info->debug.found,
		info->debug.itermax);
	fprintf(stderr, "Tests       : %lu (%0.2f per search, %0.2f per iter., %0.2f before found, %lu max)\n",
		info->debug.tests, (double) info->debug.tests / info->debug.searches,
		(double) info->debug.tests / info->debug.iterations,
		(double) info->debug.testsdelta / info->debug.found,
		info->debug.testsmax);
	fprintf(stderr, "Total keylen: %lu bytes (%0.2f per search, %0.2f per iter.)\n",
		info->debug.total_key_len, (double) info->debug.total_key_len / info->debug.searches,
		(double) info->debug.total_key_len / info->debug.iterations);
	fprintf(stderr, "\n");
}

#else /* !DEBUG_FASTFIND */

#define FF_DBG_mem(x, size)
#define FF_DBG_test(x)
#define FF_DBG_iter(x)
#define FF_DBG_found(x)
#define FF_DBG_comment(x, comment)
#define FF_DBG_search_stats(info, key_len)
#define FF_DBG_dump_stats(info)

#endif /* DEBUG_FASTFIND */


static struct fastfind_info *
init_fastfind(int case_sensitive, unsigned char *comment)
{
	struct fastfind_info *info = mem_calloc(1, sizeof(struct fastfind_info));

	if (!info) return NULL;

	FF_DBG_mem(info, sizeof(struct fastfind_info) - sizeof(info->debug));
	FF_DBG_comment(info, comment);

	/* Non sense to use that code if key length > 255 so... */
	info->min_key_len = 255;
	info->case_sensitive = case_sensitive;

	return info;
}

/* Return 1 on success, 0 on allocation failure */
static int
alloc_pointers(struct fastfind_info *info)
{
	void **pointers;
	int *keylen_list;

	assert(info->count < FF_MAX_KEYS);
	if_assert_failed return 0;

	/* On error, cleanup is done by fastfind_done(). */

	pointers = mem_calloc(info->count, sizeof(void *));
	if (!pointers) return 0;
	info->pointers = pointers;
	FF_DBG_mem(info, info->count * sizeof(void *));

	keylen_list = mem_calloc(info->count, sizeof(int));
	if (!keylen_list) return 0;
	info->keylen_list = keylen_list;
	FF_DBG_mem(info, info->count * sizeof(int));

	return 1;
}

/* Add pointer and its key length to correspondant arrays, incrementing
 * internal counter. */
static void
add_to_pointers(void *p, int key_len, struct fastfind_info *info)
{
	/* Record new pointer and key len, used in search */
	info->pointers[info->pointers_count] = p;
	info->keylen_list[info->pointers_count] = key_len;
	info->pointers_count++;
}

/* Return 1 on success, 0 on allocation failure */
static int
alloc_leafset(struct fastfind_info *info)
{
	struct ff_node **leafsets;
	struct ff_node *leafset;

	assert(info->leafsets_count < FF_MAX_LEAFSETS);
	if_assert_failed return 0;

	/* info->leafsets[0] is never used since l=0 marks no leaf
	 * in struct ff_node. That's the reason of that + 2. */
	leafsets = mem_realloc(info->leafsets,
			       sizeof(struct ff_node *)
			       * (info->leafsets_count + 2));
	if (!leafsets) return 0;
	info->leafsets = leafsets;

	leafset = mem_calloc(info->uniq_chars_count,
			     sizeof(struct ff_node));
	if (!leafset) return 0;

	FF_DBG_mem(info, sizeof(struct ff_node *));
	FF_DBG_mem(info, sizeof(struct ff_node) * info->uniq_chars_count);

	info->leafsets_count++;
	info->leafsets[info->leafsets_count] = leafset;

	return 1;
}

static inline int
char2idx(unsigned char c, struct fastfind_info *info)
{
	unsigned char *idx = memchr(info->uniq_chars, c, info->uniq_chars_count);

	if (idx) return (idx - info->uniq_chars);

	return -1;
}

static inline void
init_idxtab(struct fastfind_info *info)
{
	int i;

	for (i = 0; i < FF_MAX_CHARS; i++)
		info->idxtab[i] = char2idx((unsigned char) i, info);
}

#define ifcase(c) ((info->case_sensitive) ? (c) : toupper(c))

struct fastfind_info *
fastfind_index(void (*reset)(void), struct fastfind_key_value *(*next)(void),
	       int case_sensitive, unsigned char *comment)
{
	struct fastfind_key_value *p;
	struct fastfind_info *info = init_fastfind(case_sensitive, comment);

	if (!info) goto alloc_error;

	assert(reset && next);
	if_assert_failed goto alloc_error;

	/* First search min, max, count and uniq_chars. */
	(*reset)();
	while ((p = (*next)())) {
		int key_len = strlen(p->key);
		int i;

		assert(key_len); /* We do not want empty keys. */
		if_assert_failed goto alloc_error;

		if (key_len < info->min_key_len)
			info->min_key_len = key_len;

		if (key_len > info->max_key_len)
			info->max_key_len = key_len;

		for (i = 0; i < key_len; i++) {
			/* ifcase() test should be moved outside loops but
			 * remember we call this routine only once per list.
			 * So I go for code readability vs performance here.
			 * --Zas */
			int k = ifcase(p->key[i]);

			assert(k < FF_MAX_CHARS);
			if_assert_failed goto alloc_error;

			if (char2idx(k, info) == -1) {
				assert(info->uniq_chars_count < FF_MAX_CHARS);
				if_assert_failed goto alloc_error;

				info->uniq_chars[info->uniq_chars_count++] = k;
			}
		}

		info->count++;
	}

	if (!info->count) return 0;

	init_idxtab(info);

	/* Root leafset allocation */
	if (!alloc_leafset(info)) goto alloc_error;

	info->root_leafset = info->leafsets[info->leafsets_count];

	if (!alloc_pointers(info)) goto alloc_error;

	/* Build the tree */
	(*reset)();
	while ((p = (*next)())) {
		int key_len = strlen(p->key);
		struct ff_node *leafset = info->root_leafset;
		int i;

#if 0
		fprintf(stderr, "K: %s\n", p->key);
#endif
		for (i = 0; i < key_len - 1; i++) {
			/* Convert char to its index value */
			int idx = info->idxtab[ifcase(p->key[i])];

			/* leafset[idx] is the desired leaf node's bucket. */

			if (leafset[idx].l == 0) {
				/* There's no leaf yet */
				if (!alloc_leafset(info)) goto alloc_error;
				leafset[idx].l = info->leafsets_count;
			}

			/* Descend to leaf */
			leafset = info->leafsets[leafset[idx].l];
		}

		/* Index final leaf */
		i = info->idxtab[ifcase(p->key[i])];

		leafset[i].e = 1;

		/* Memorize pointer to data */
		leafset[i].p = info->pointers_count;
		add_to_pointers(p->data, key_len, info);
	}

	return info;

alloc_error:
	fastfind_done(info);
	return NULL;
}

#undef ifcase

void
fastfind_node_compress(struct ff_node *leafset, struct fastfind_info *info)
{
	int cnt = 0;
	int pos = 0;
	int i;

	assert(info);
	if_assert_failed return;

	for (i = 0; i < info->uniq_chars_count; i++) {
		if (leafset[i].c) continue;

		if (leafset[i].l) {
			/* There's a leaf leafset, descend to it and recurse */
			fastfind_node_compress(info->leafsets[leafset[i].l],
					       info);
		}

		if (leafset[i].l || leafset[i].e) {
			cnt++;
			pos = i;
		}
	}

	if (cnt != 1 || leafset[pos].c) return;

	/* Compress if possible ;) */
	for (i = 1; i < info->leafsets_count; i++)
		if (info->leafsets[i] == leafset)
			break;

	if (i < info->leafsets_count) {
		struct ff_node_c *new = mem_alloc(sizeof(struct ff_node_c));

		if (!new) return;

		new->c = 1;
		new->e = leafset[pos].e;
		new->p = leafset[pos].p;
		new->l = leafset[pos].l;
		new->ch = pos;

		mem_free(info->leafsets[i]);
		info->leafsets[i] = (struct ff_node *) new;
		FF_DBG_mem(info, sizeof(struct ff_node_c));
		FF_DBG_mem(info, sizeof(struct ff_node) * -info->uniq_chars_count);
	}
}

void
fastfind_index_compress(struct fastfind_info *info)
{
	assert(info);
	if_assert_failed return;
	fastfind_node_compress(info->root_leafset, info);
}


/* This macro searchs for the key in indexed list */
#define FF_SEARCH(what) do {							\
	int i;									\
										\
	for (i = 0; i < key_len; i++) {						\
		int lidx, k = what;						\
										\
		FF_DBG_iter(info);						\
										\
		FF_DBG_test(info);						\
		if (k >= FF_MAX_CHARS) return NULL;				\
		lidx = info->idxtab[k];						\
										\
		FF_DBG_test(info);						\
		if (lidx < 0) return NULL;					\
										\
		FF_DBG_test(info);						\
		if (current->c) {						\
			/* It is a compressed leaf. */				\
			FF_DBG_test(info);					\
			if (((struct ff_node_c *) current)->ch != lidx)		\
				return NULL;					\
		} else {							\
			current = &current[lidx];				\
		}								\
										\
		FF_DBG_test(info);						\
		if (current->e && key_len == info->keylen_list[current->p]) {	\
			FF_DBG_test(info);					\
			FF_DBG_found(info);					\
			return info->pointers[current->p];			\
		}								\
										\
		FF_DBG_test(info);						\
		if (!current->l) return NULL;					\
		current = (struct ff_node *) info->leafsets[current->l];	\
	}									\
} while (0)

void *
fastfind_search(unsigned char *key, int key_len, struct fastfind_info *info)
{
	struct ff_node *current;

	assert(info);
	if_assert_failed return NULL;

	FF_DBG_search_stats(info, key_len);

	FF_DBG_test(info); if (!key) return NULL;
	FF_DBG_test(info); if (key_len > info->max_key_len) return NULL;
	FF_DBG_test(info); if (key_len < info->min_key_len) return NULL;

	current = info->root_leafset;

	/* Macro and code redundancy are there to obtain maximum
	 * performance. Do not move it to an inlined function.
	 * Do not even think about it.
	 * If you find a better way (same or better performance) then
	 * propose it and be prepared to defend it. --Zas */

	FF_DBG_test(info);
	if (info->case_sensitive)
		FF_SEARCH(key[i]);
	else
		FF_SEARCH(toupper(key[i]));

	return NULL;
}

#undef FF_SEARCH

void
fastfind_done(struct fastfind_info *info)
{
	if (!info) return;

	FF_DBG_dump_stats(info);

	mem_free_if(info->pointers);
	mem_free_if(info->keylen_list);
	while (info->leafsets_count) {
		mem_free_if(info->leafsets[info->leafsets_count]);
		info->leafsets_count--;
	}
	mem_free_if(info->leafsets);
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
	struct fastfind_info *info;

	if (!key || !*key) {
		fprintf(stderr, "Usage: fastfind keyword\n");
		exit(-1);
	}

	fprintf(stderr, "---------- INDEX PHASE -----------\n");
	/* Mandatory */
	info = fastfind_index(&reset_list, &next_in_list);

	fprintf(stderr, "--------- COMPRESS PHASE ---------\n");
	/* Highly recommended but optional. */
	fastfind_index_compress(info);

	fprintf(stderr, "---------- SEARCH PHASE ----------\n");
	/* Without this one ... */
	result = (struct list *) fastfind_search(key, strlen(key), info);

	if (result)
		fprintf(stderr, " Found: '%s' -> %d\n", result->tag, result->val);
	else
		fprintf(stderr, " Not found: '%s'\n", key);

	fastfind_done(info);

	exit(0);
}

#endif

#endif /* USE_FASTFIND */
