/* Very fast search_keyword_in_list. */
/* $Id: fastfind.c,v 1.45 2003/10/16 13:08:47 zas Exp $ */

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
#define FASTFIND_DEBUG
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

#define POINTER_INDEX_BITS	9	/* 512 */
#define LEAFSET_INDEX_BITS	14	/* 16384 */
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


#define FF_MAX_KEYS  (1  << POINTER_INDEX_BITS)
#define FF_MAX_LEAFSETS ((1 << LEAFSET_INDEX_BITS) - 1)
#define FF_MAX_CHARS (1  << COMP_CHAR_INDEX_BITS)


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

#ifdef FASTFIND_DEBUG
	unsigned long searches;
	unsigned long found;
	unsigned long itertmp;
	unsigned long iterdelta;
	unsigned long iterations;
	unsigned long tests;
	unsigned long teststmp;
	unsigned long testsdelta;
	unsigned long memory_usage;
	unsigned long total_key_len;
#endif

	unsigned char uniq_chars[FF_MAX_CHARS];
};


#ifdef FASTFIND_DEBUG
/* These are for performance testing. */
#define meminc(x, size) (x)->memory_usage += (size)
#define testinc(x) (x)->tests++
#define iterinc(x) (x)->iterations++
#define foundinc(x) \
	do { \
		(x)->iterdelta += (x)->iterations - (x)->itertmp;	\
		(x)->testsdelta += (x)->tests - (x)->teststmp;		\
		(x)->found++;						\
	} while (0)
/* ACCounted IF ;-) */
#define accif(x) testinc(x); if

#else /* !FASTFIND_DEBUG */

#define meminc(x, size)
#define testinc(x)
#define iterinc(x)
#define foundinc(x)
#define accif(x) if

#endif


static struct fastfind_info *
init_fastfind(int case_sensitive)
{
	struct fastfind_info *info = mem_calloc(1, sizeof(struct fastfind_info));

	if (!info) return NULL;

	meminc(info, sizeof(struct fastfind_info) - sizeof(unsigned long) * 10);
	/* Non sense to use that code if key length > 255 so... */
	info->min_key_len = 255;
	info->case_sensitive = case_sensitive;

	return info;
}

/* Return 1 on success, 0 on allocation failure */
static int
add_to_pointers(void *p, int key_len, struct fastfind_info *info)
{
	void **pointers;
	int *keylen_list;
	int new_count = info->pointers_count + 1;

	assert(new_count < FF_MAX_KEYS);
	if_assert_failed return 0;

	/* On error, cleanup is done by fastfind_done(). */

	pointers = mem_realloc(info->pointers, new_count * sizeof(void *));
	if (!pointers) return 0;
	info->pointers = pointers;

	keylen_list = mem_realloc(info->keylen_list, new_count * sizeof(int));
	if (!keylen_list) return 0;
	info->keylen_list = keylen_list;

	meminc(info, sizeof(int) + sizeof(void *));

	/* Record new pointer and key len, used in search */
	info->pointers[info->pointers_count] = p;
	info->keylen_list[info->pointers_count] = key_len;

	info->pointers_count = new_count;

	return 1;
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

	meminc(info, sizeof(struct ff_node *));
	meminc(info, sizeof(struct ff_node) * info->uniq_chars_count);

	info->leafsets_count++;
	info->leafsets[info->leafsets_count] = leafset;

	return 1;
}

static int
char2idx(unsigned char c, struct fastfind_info *info)
{
	register int idx;

	for (idx = 0; idx < info->uniq_chars_count; idx++)
		if (info->uniq_chars[idx] == c)
			return idx;

	return -1;
}

static void
init_idxtab(struct fastfind_info *info)
{
	register int i;

	for (i = 0; i < FF_MAX_CHARS; i++)
		info->idxtab[i] = char2idx((unsigned char) i, info);
}

#define ifcase(c) (info->case_sensitive ? c : upcase(c))

struct fastfind_info *
fastfind_index(void (*reset)(void), struct fastfind_key_value *(*next)(void),
	       int case_sensitive)
{
	struct fastfind_key_value *p;
	struct fastfind_info *info = init_fastfind(case_sensitive);

	if (!info) goto alloc_error;

	assert(reset && next);
	if_assert_failed goto alloc_error;

	/* First search min, max, count and uniq_chars. */
	(*reset)();
	while ((p = (*next)())) {
		int key_len = strlen(p->key);
		register int i;

		assert(key_len); /* We do not want empty keys. */
		if_assert_failed goto alloc_error;

		if (key_len < info->min_key_len)
			info->min_key_len = key_len;

		if (key_len > info->max_key_len)
			info->max_key_len = key_len;

		for (i = 0; i < key_len; i++) {
			int j, k;

			k = ifcase(p->key[i]);

			assert(k < FF_MAX_CHARS);
			if_assert_failed goto alloc_error;

			/* ifcase() test should be moved outside loops but
			 * remember we call this routine only once per list.
			 * So I go for code readability vs performance here.
			 * --Zas */
			for (j = 0; j < info->uniq_chars_count; j++)
				if (info->uniq_chars[j] == k)
						break;

			if (j >= info->uniq_chars_count) {
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

	/* Build the tree */
	(*reset)();
	while ((p = (*next)())) {
		int key_len = strlen(p->key);
		struct ff_node *leafset = info->root_leafset;
		register int i;

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
		if (!add_to_pointers(p->data, key_len, info))
			goto alloc_error;
	}

	return info;

alloc_error:
	fastfind_done(info);
	return NULL;
}

void
fastfind_node_compress(struct ff_node *leafset, struct fastfind_info *info)
{
	int cnt = 0;
	int pos = 0;
	register int i = 0;

	assert(info);
	if_assert_failed return;

	for (; i < info->uniq_chars_count; i++) {
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
		meminc(info, sizeof(struct ff_node_c));
		meminc(info, sizeof(struct ff_node) * -info->uniq_chars_count);
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
#define FF_SEARCH(what)								\
	register int i = 0;							\
										\
	for (; i < key_len; i++) {						\
		int lidx, k = what;						\
										\
		iterinc(info);							\
										\
		accif(info) (k >= FF_MAX_CHARS) return NULL;			\
		lidx = info->idxtab[k];						\
										\
		accif(info) (lidx < 0) return NULL;				\
										\
		accif(info) (current->c) {					\
			/* It is a compressed leaf. */				\
			accif(info) (((struct ff_node_c *) current)->ch != lidx)\
				return NULL;					\
		} else {							\
			current = &current[lidx];				\
		}								\
										\
		accif(info) (current->e						\
			     && key_len == info->keylen_list[current->p]) {	\
			testinc(info);						\
			foundinc(info);						\
			return info->pointers[current->p];			\
		}								\
										\
		accif(info) (!current->l)					\
				return NULL;					\
		current = (struct ff_node *) info->leafsets[current->l];	\
	}

void *
fastfind_search(unsigned char *key, int key_len, struct fastfind_info *info)
{
	struct ff_node *current;

	assert(info);
	if_assert_failed return NULL;

#ifdef FASTFIND_DEBUG
	info->searches++;
	info->total_key_len += key_len;
	info->teststmp = info->tests;
	info->itertmp = info->iterations;
#endif

   	accif(info) (!key) return NULL;
	accif(info) (key_len > info->max_key_len) return NULL;
	accif(info) (key_len < info->min_key_len) return NULL;

	current = info->root_leafset;

	/* Macro and code redundancy are there to obtain maximum
	 * performance. Do not move it to an inlined function.
	 * Do not even think about it.
	 * If you find a better way (same or better performance) then
	 * propose it and be prepared to defend it. --Zas */

	accif(info) (info->case_sensitive) {
		FF_SEARCH(key[i]);
	} else {
		FF_SEARCH(upcase(key[i]));
	}

	return NULL;
}

#undef FF_SEARCH

void
fastfind_done(struct fastfind_info *info)
{
	if (!info) return;

#ifdef FASTFIND_DEBUG
	fprintf(stderr, "------ FastFind Statistics ------\n");
	fprintf(stderr, "Uniq_chars  : %s\n", info->uniq_chars);
	fprintf(stderr, "Uniq_chars #: %d/%d max.\n", info->uniq_chars_count, FF_MAX_CHARS);
	fprintf(stderr, "Min_key_len : %d\n", info->min_key_len);
	fprintf(stderr, "Max_key_len : %d\n", info->max_key_len);
	fprintf(stderr, "Entries     : %d/%d max.\n", info->pointers_count, FF_MAX_KEYS);
	fprintf(stderr, "FFleafsets  : %d/%d max.\n", info->leafsets_count, FF_MAX_LEAFSETS);
	fprintf(stderr, "Memory usage: %lu bytes (cost per entry = %0.2f bytes)\n",
		info->memory_usage, (double) info->memory_usage / info->pointers_count);
	fprintf(stderr, "Struct node : %d bytes (normal) , %d bytes (compressed)\n",
		sizeof(struct ff_node), sizeof(struct ff_node_c));
	fprintf(stderr, "Searches    : %lu\n", info->searches);
	fprintf(stderr, "Found       : %lu (%0.2f%%)\n",
		info->found, 100 * (double) info->found / info->searches);
	fprintf(stderr, "Iterations  : %lu (%0.2f per search, %0.2f before found)\n",
		info->iterations, (double) info->iterations / info->searches,
		(double) info->iterdelta / info->found);
	fprintf(stderr, "Tests       : %lu (%0.2f per search, %0.2f per iter., %0.2f before found)\n",
		info->tests, (double) info->tests / info->searches,
		(double) info->tests / info->iterations,
		(double) info->testsdelta / info->found);
	fprintf(stderr, "Total keylen: %lu bytes (%0.2f per search, %0.2f per iter.)\n",
		info->total_key_len, (double) info->total_key_len / info->searches,
		(double) info->total_key_len / info->iterations);
#endif

	if (info->pointers) mem_free(info->pointers);
	if (info->keylen_list) mem_free(info->keylen_list);
	while (info->leafsets_count) {
		if (info->leafsets[info->leafsets_count])
			mem_free(info->leafsets[info->leafsets_count]);
		info->leafsets_count--;
	}
	if (info->leafsets) mem_free(info->leafsets);
	mem_free(info);
}

#undef ifcase


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
