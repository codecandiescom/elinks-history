/* Base64 encoder implementation. */
/* $Id: base64.c,v 1.9 2003/08/01 19:37:20 zas Exp $ */

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
	unsigned int val = 0;
	unsigned int tmp;

	assert(in && *in);

	outstr = out = mem_alloc(strlen(in) / 4 * 3 + 1);
	if (!outstr) return NULL;

	while (*in) {
	        val = INDEXOF(*in++) << 18 | INDEXOF(*in++) << 12;
	        if (*in != '=') val |= INDEXOF(*in) << 6;
		in++;
	        if (*in != '=') val |= INDEXOF(*in);
		in++;
	        *out++ = val >> 16;
	        tmp = (val & 0xFF00) >> 8;
	        if (tmp) *out++ = tmp;
	        tmp = (val & 0xFF);
	        if (tmp) *out++ = tmp;
	}
	*out = '\0';

	return outstr;
}
#endif /* FORMS_MEMORY */
