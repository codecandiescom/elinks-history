/* $Id: fastfind.h,v 1.4 2003/06/14 17:21:59 zas Exp $ */

#ifndef EL__UTIL_FASTFIND_H
#define EL__UTIL_FASTFIND_H

struct fastfind_key_value {
	unsigned char *key;
	void *data;
};

/* Initialize and index a list of keys. */
/* Keys are iterated using:
 * @reset		to start over and
 * @next		to get next struct fastfind_key_value in line.
 * @case_sensitive	denotes whether to honour case when comparing. */
/* This function must be called once and only once per list and
 * returns a handle to the allocated structure. */
void *fastfind_index(void (*reset)(void),
		     struct fastfind_key_value *(*next)(void),
		     int case_sensitive);

/* This one should be called to minimize memory usage of index. */
/* @current		is the element to compress for internal use.
 *			By passing NULL the whole index is compressed.
 * @fastfind_handle	is the handle created with fastfind_index(). */
/* Highly recommended but optionnal. */
void fastfind_index_compress(void *current /* =NULL */, void *fastfind_handle);

/* The main reason of all that stuff is here. */
/* Search the index for @key with length @key_len using the
 * @fastfind_info handle created with fastfind_index(). */
void *fastfind_search(unsigned char *key, int key_len, void *fastfind_handle);

/* Fastfind cleanup. It frees the index given by the @fastfind_handle. */
/* Must be called once per list. */
void fastfind_terminate(void *fastfind_handle);

#endif /* EL__UTIL_FASTFIND_H */
