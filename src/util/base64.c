/* Base64 encoder implementation. */
/* $Id: base64.c,v 1.6 2003/07/24 13:38:49 zas Exp $ */

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
		*out++ = base64_chars[ ((*in << 4 | *(++in) >> 4) & 63) ];
		*out++ = base64_chars[ ((*in << 2 | *(++in) >> 6) & 63) ];
		*out++ = base64_chars[ (*in & 63) ];
		in++;
		inlen -= 3;
	}
	if (inlen == 1) {
		*out++ = base64_chars[ (*in >> 2) ];
		*out++ = base64_chars[ (*in << 4 & 63) ];
		*out++ = '=';
		*out++ = '=';
	}
	if (inlen == 2) {
		*out++ = base64_chars[ (*in >> 2) ];
		*out++ = base64_chars[ ((*in << 4 | *(++in) >> 4) & 63) ];
		*out++ = base64_chars[ ((*in << 2) & 63) ];
		*out++ = '=';
	}
	*out = 0;

	return outstr;
}
