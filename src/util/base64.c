/* Base64 encoder implementation. */
/* $Id: base64.c,v 1.4 2002/09/11 15:58:28 zas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "links.h"

#include "util/base64.h"
#include "util/memory.h"

unsigned char base64_chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

unsigned char *
base64_encode(unsigned char *in)
{
	unsigned char *out, *outstr;
	int inlen = strlen(in);

	outstr = out = mem_alloc(((inlen / 3) + 1) * 4 + 1 );
	if (!outstr) return NULL;

	while (inlen >= 3) {
		*out++ = base64_chars[(int) (*in >> 2) ];
		*out++ = base64_chars[(int) ((*in << 4 | *(in + 1) >> 4) & 63) ];
		*out++ = base64_chars[(int) ((*(in + 1) << 2 | *(in + 2) >> 6) & 63) ];
		*out++ = base64_chars[(int) (*(in + 2) & 63) ];
		inlen -= 3; in += 3;
	}
	if (inlen == 1) {
		*out++ = base64_chars[(int) (*in >> 2) ];
		*out++ = base64_chars[(int) (*in << 4 & 63) ];
		*out++ = '=';
		*out++ = '=';
	}
	if (inlen == 2) {
		*out++ = base64_chars[(int) (*in >> 2) ];
		*out++ = base64_chars[(int) ((*in << 4 | *(in + 1) >> 4) & 63) ];
		*out++ = base64_chars[(int) ((*(in + 1) << 2) & 63) ];
		*out++ = '=';
	}
	*out = 0;

	return outstr;
}
