/* $Id: fastfind.h,v 1.2 2003/06/13 17:02:36 zas Exp $ */

#ifndef EL__UTIL_FASTFIND_H
#define EL__UTIL_FASTFIND_H

struct fastfind_key_value {
	unsigned char *key;
	void *data;
};

void *fastfind_index(void (*reset) (void), struct fastfind_key_value * (*next) (void), int case_sensitive);
void fastfind_index_compress(void *current /* =NULL */, void *ff_info_);
void *fastfind_search(unsigned char *key, int key_len, void *ff_info_);
void fastfind_terminate(void *ff_info_);


/* Statistics */
/* #define FASTFIND_DEBUG */

#endif /* EL__UTIL_FASTFIND_H */
