/* Gzip encoding (ENCODING_GZIP) backend */
/* $Id: gzip.c,v 1.1 2004/05/28 11:55:26 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_ZLIB_H
#include <zlib.h>
#endif

#include "elinks.h"

#include "encoding/encoding.h"
#include "encoding/gzip.h"
#include "util/memory.h"


static int
gzip_open(struct stream_encoded *stream, int fd)
{
	stream->data = (void *) gzdopen(fd, "rb");
	if (!stream->data) return -1;

	return 0;
}

static int
gzip_read(struct stream_encoded *stream, unsigned char *data, int len)
{
	return gzread((gzFile *) stream->data, data, len);
}

static unsigned char *
gzip_decode(struct stream_encoded *stream, unsigned char *data, int len,
	    int *new_len)
{
	*new_len = len;
	return data;
}

static void
gzip_close(struct stream_encoded *stream)
{
	gzclose((gzFile *) stream->data);
}

struct decoding_backend gzip_decoding_backend = {
	"gzip",
	gzip_open,
	gzip_read,
	gzip_decode,
	gzip_close,
	{ ".gz", ".tgz", NULL },
};
