/* $Id: fastfind.h,v 1.17 2004/10/27 17:17:46 zas Exp $ */

#ifndef EL__UTIL_FASTFIND_H
#define EL__UTIL_FASTFIND_H

/* Whether to use these routines or not. */
#ifndef CONFIG_SMALL
#define USE_FASTFIND 1
#else
#undef USE_FASTFIND
#endif

#ifdef USE_FASTFIND

struct fastfind_info;

struct fastfind_key_value {
	unsigned char *key;
	void *data;
};

enum fastfind_flags {
	FF_NONE = 0,
	FF_CASE_AWARE = 1,	/* honour case when comparing */
	FF_COMPRESS = 2,	/* compress nodes if possible */
};

/* Initialize and index a list of keys. */
/* Keys are iterated using:
 * @reset		to start over and
 * @next		to get next struct fastfind_key_value in line.
 * @flags		control case sensitivity, compression.
 * @comment		useful for debugging mode. */
/* This function must be called once and only once per list and
 * returns a handle to the allocated structure. */
struct fastfind_info *fastfind_index(void (*reset)(void),
		struct fastfind_key_value *(*next)(void),
		enum fastfind_flags flags,
		unsigned char *comment);

/* The main reason of all that stuff is here. */
/* Search the index for @key with length @key_len using the
 * @fastfind_info handle created with fastfind_index(). */
void *fastfind_search(unsigned char *key, int key_len,
		struct fastfind_info *fastfind_handle);

/* Fastfind cleanup. It frees the index given by the @fastfind_handle. */
/* Must be called once per list. */
void fastfind_done(struct fastfind_info *fastfind_handle);

#endif

#endif /* EL__UTIL_FASTFIND_H */
