/* Base64 encoder implementation. */
/* $Id: base64.c,v 1.7 2003/07/25 00:39:28 jonas Exp $ */

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
