/* Base64 encoder implementation. */
/* $Id: base64.c,v 1.8 2003/08/01 17:28:38 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "elinks.h"

#include "util/base64.h"
#include "util/error.h"
#include "util/memory.h"

unsigned char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

unsigned char *
base64_encode(unsigned char *str)
{
	register unsigned char *in = str;
	register unsigned char *out;
	unsigned char *outstr;
	int inlen;

	assert(str && *str);

	inlen = strlen(in);
	out = outstr = mem_alloc((inlen / 3) * 4 + 4 + 1);
	if (!out) return NULL;

	while (inlen >= 3) {
		*out++ = base64_chars[ (*in >> 2) ];
		*out++ = base64_chars[ ((*in << 4 | *(in + 1) >> 4) & 63) ];
		*out++ = base64_chars[ ((*(in + 1) << 2 | *(in + 2) >> 6) & 63) ];
		*out++ = base64_chars[ (*(in + 2) & 63) ];
		inlen -= 3; in += 3;
	}
	if (inlen == 1) {
		*out++ = base64_chars[ (*in >> 2) ];
		*out++ = base64_chars[ (*in << 4 & 63) ];
		*out++ = '=';
		*out++ = '=';
	}
	if (inlen == 2) {
		*out++ = base64_chars[ (*in >> 2) ];
		*out++ = base64_chars[ ((*in << 4 | *(in + 1) >> 4) & 63) ];
		*out++ = base64_chars[ ((*(in + 1) << 2) & 63) ];
		*out++ = '=';
	}
	*out = 0;

	return outstr;
}

/* Base64 decoding is used only with the FORMS_MEMORY feature, so i'll #ifdef it */
#ifdef FORMS_MEMORY
#define INDEXOF(x) ((unsigned char *) strchr(base64_chars, (x)) - base64_chars)
/* base64_decode:  @in string to decode
 *                 returns the string decoded (must be freed by the caller) */
unsigned char *
base64_decode(unsigned char *in)
{
	unsigned char *out, *outstr;
	unsigned int buffer = 0;
	int tmp;
	int inlen = strlen(in);

	assert(in && *in);

	outstr = out = mem_alloc(inlen / 4 * 3 + 1);
	if (!outstr) return NULL;

	while (*in) {
	        buffer = INDEXOF(*in++) << 18;
	        buffer |= INDEXOF(*in++) << 12;
	        if (*in++ != '=') buffer |= INDEXOF(*(in - 1)) << 6;
	        if (*in++ != '=') buffer |= INDEXOF(*(in - 1));
	        *out++ = buffer >> 16;
	        tmp = (buffer & 0xFF00) >> 8;
	        if (tmp) *out++ = tmp;
	        tmp = (buffer & 0xFF);
	        if (tmp) *out++ = tmp;
	        buffer = 0;
	 }
	*out = '\0';
	return outstr;
}
#endif /* FORMS_MEMORY */
