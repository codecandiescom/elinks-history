/* Bzip2 encoding (ENCODING_BZIP2) backend */
/* $Id: bzip2.c,v 1.2 2004/05/28 13:02:30 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef HAVE_BZLIB_H
#include <bzlib.h> /* Everything needs this after stdio.h */
#endif

#include "elinks.h"

#include "encoding/bzip2.h"
#include "encoding/encoding.h"
#include "util/memory.h"


struct bz2_enc_data {
	FILE *file;
	BZFILE *bzfile;
	int last_read; /* If err after last bzRead() was BZ_STREAM_END.. */
};

/* TODO: When it'll be official, use bzdopen() from Yoshioka Tsuneo. --pasky */

static int
bzip2_open(struct stream_encoded *stream, int fd)
{
	struct bz2_enc_data *data = mem_alloc(sizeof(struct bz2_enc_data));
	int err;

	if (!data) {
		return -1;
	}
	data->last_read = 0;

	data->file = fdopen(fd, "rb");

	data->bzfile = BZ2_bzReadOpen(&err, data->file, 0, 0, NULL, 0);
	if (!data->bzfile) {
		mem_free(data);
		return -1;
	}

	stream->data = data;

	return 0;
}

static int
bzip2_read(struct stream_encoded *stream, unsigned char *buf, int len)
{
	struct bz2_enc_data *data = (struct bz2_enc_data *) stream->data;
	int err = 0;

	if (data->last_read)
		return 0;

	len = BZ2_bzRead(&err, data->bzfile, buf, len);

	if (err == BZ_STREAM_END)
		data->last_read = 1;
	else if (err)
		return -1;

	return len;
}

static unsigned char *
bzip2_decode(struct stream_encoded *stream, unsigned char *data, int len,
	     int *new_len)
{
	*new_len = len;
	return data;
}

static void
bzip2_close(struct stream_encoded *stream)
{
	struct bz2_enc_data *data = (struct bz2_enc_data *) stream->data;
	int err;

	BZ2_bzReadClose(&err, data->bzfile);
	fclose(data->file);
	mem_free(data);
}

static unsigned char *bzip2_extensions[] = { ".bz2", ".tbz", NULL };

struct decoding_backend bzip2_decoding_backend = {
	"bzip2",
	bzip2_extensions,
	bzip2_open,
	bzip2_read,
	bzip2_decode,
	bzip2_close,
};
