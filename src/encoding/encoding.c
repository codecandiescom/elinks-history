/* Stream reading and decoding (mostly decompression) */
/* $Id: encoding.c,v 1.1 2002/06/11 19:27:18 pasky Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>
#ifdef HAVE_BZLIB_H
#include <bzlib.h> /* Everything needs this after stdio.h */
#endif
#ifdef HAVE_ZLIB_H
#include <zlib.h>
#endif

/* XXX: This seems (b)zlib and OpenSSL are incompatible. Since we don't need
 * OpenSSL here, we try to draw it off the game this way. */
#define HEADER_SSL_H

#include "links.h"

#include "util/encoding.h"


/* TODO: When more decoders will join the game, we should probably move them
 * to separate files, maybe even to separate directory. --pasky */


#if 0
static void *
z_mem_alloc(void *opaque, int items, int size)
{
	return mem_alloc(items * size);
}

static void
z_mem_free(void *opaque, void *address)
{
	mem_free(address);
}
#endif


struct decoding_handlers {
	int (*open)(struct stream_encoded *stream, int fd);
	int (*read)(struct stream_encoded *stream, unsigned char *data, int len);
	void (*close)(struct stream_encoded *stream);
};


/*************************************************************************
  Dummy encoding (ENCODING_NONE)
*************************************************************************/

struct dummy_enc_data {
	int fd;
};

static int
dummy_open(struct stream_encoded *stream, int fd)
{
	stream->data = mem_alloc(sizeof(struct dummy_enc_data));
	if (!stream->data) return -1;

	((struct dummy_enc_data *) stream->data)->fd = fd;

	return 0;
}

static int
dummy_read(struct stream_encoded *stream, unsigned char *data, int len)
{
	return read(((struct dummy_enc_data *) stream->data)->fd, data, len);
}

static void
dummy_close(struct stream_encoded *stream)
{
	close(((struct dummy_enc_data *) stream->data)->fd);
	mem_free(stream->data);
}

struct decoding_handlers dummy_handlers = {
	dummy_open,
	dummy_read,
	dummy_close,
};


/*************************************************************************
  Gzip encoding (ENCODING_GZIP)
*************************************************************************/

#ifdef HAVE_ZLIB_H

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

static void
gzip_close(struct stream_encoded *stream)
{
	gzclose((gzFile *) stream->data);
}

#if 0
static unsigned char *
decompress_gzip(unsigned char *stream, int cur_size, int *new_size)
{
	z_stream z;
	char *stream_pos = stream;
	char method, flags;
	char *output;
	int size;
	int ret;

	output = mem_alloc(cur_size * 4);
	if (!output) return stream;

	z.opaque = NULL;
	z.zalloc = (alloc_func) z_mem_alloc;
	z.zfree = z_mem_free;
	z.next_in = stream_pos;
	z.next_out = output;
	z.avail_out = size = cur_size * 4;
	z.avail_in = cur_size + 1;

	/* XXX: Why -15? --pasky */
	ret = inflateInit2(&z, -15);

	while (ret == Z_OK) {
		char *output_new;

		ret = inflate(&z, Z_SYNC_FLUSH);

		if (ret == Z_STREAM_END) {
			mem_free(stream);
			*new_size = (int) z.total_out;
			output = mem_realloc(output, z.total_out);
			inflateEnd(&z);
			return output;
		}

		if (ret != Z_OK) {
			inflateEnd(&z);
			break;
		}

		size += cur_size * 4;

		output_new = mem_realloc(output, size);
		if (!output_new) {
			mem_free(output);
			inflateEnd(&z);
			return stream;
		}

		output = output_new;
		z.avail_out += cur_size * 4;
		z.next_out = output + z.total_out;
	}

	mem_free(output);
	
	return stream;
}
#endif

struct decoding_handlers gzip_handlers = {
	gzip_open,
	gzip_read,
	gzip_close,
};

#endif


/*************************************************************************
  Bzip2 encoding (ENCODING_BZIP2)
*************************************************************************/

#ifdef HAVE_BZLIB_H

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

static void
bzip2_close(struct stream_encoded *stream)
{
	struct bz2_enc_data *data = (struct bz2_enc_data *) stream->data;
	int err;

	BZ2_bzReadClose(&err, data->bzfile);
	fclose(data->file);
	mem_free(data);
}

#if 0
static unsigned char *
decompress_bzip2(unsigned char *stream, int cur_size, int *new_size)
{
	bz_stream bz;
	char *output;
	int size;
	int ret;
	
	output = mem_alloc(cur_size * 4);
	if (!output) return stream;
	
	bz.opaque = NULL;
	bz.bzalloc = z_mem_alloc;
	bz.bzfree = z_mem_free;
	bz.next_in = stream;
	bz.next_out = output;
	bz.avail_out = size = cur_size * 4;
	bz.avail_in = cur_size;

	ret = BZ2_bzDecompressInit(&bz, 0, 0);

	while (ret == BZ_OK) {
		char *output_new;

		ret = BZ2_bzDecompress(&bz);

		if (ret == BZ_STREAM_END) {
			mem_free(stream);
			*new_size = (int) bz.total_out_lo32;
			output = mem_realloc(output, bz.total_out_lo32);
			BZ2_bzDecompressEnd(&bz);
			return output;
		}

		if (ret != BZ_OK) {
			BZ2_bzDecompressEnd(&bz);
			break;
		}

		size += cur_size * 4;

		output_new = mem_realloc(output, size);
		if (!output_new) {
			mem_free(output);
			BZ2_bzDecompressEnd(&bz);
			return stream;
		}

		output = output_new;
		bz.avail_out += cur_size * 4;
		bz.next_out = output+bz.total_out_lo32;
	}

	mem_free(output);

	return stream;
}
#endif

struct decoding_handlers bzip2_handlers = {
	bzip2_open,
	bzip2_read,
	bzip2_close,
};

#endif


struct decoding_handlers *handlers[] = {
	&dummy_handlers,
#ifdef HAVE_ZLIB_H
	&gzip_handlers,
#else
	&dummy_handlers,
#endif
#ifdef HAVE_BZLIB_H
	&bzip2_handlers,
#else
	&dummy_handlers,
#endif
};


/*************************************************************************
  Public functions
*************************************************************************/


/* Associates encoded stream with a fd. */
struct stream_encoded *
open_encoded(int fd, enum stream_encoding encoding)
{
	struct stream_encoded *stream;
	
	stream = mem_alloc(sizeof(struct stream_encoded));
	if (!stream) return NULL;

	stream->encoding = encoding;
	if (handlers[stream->encoding]->open(stream, fd) >= 0)
		return stream;

	mem_free(stream);
	return NULL;
}

/* Read available data from stream and decode them. Note that when data change
 * their size during decoding, 'len' indicates desired size of _returned_ data,
 * not desired size of data read from stream. */
int
read_encoded(struct stream_encoded *stream, unsigned char *data, int len)
{
	return handlers[stream->encoding]->read(stream, data, len);
}

/* Closes encoded stream. Note that fd associated with the stream will be
 * closed here. */
void
close_encoded(struct stream_encoded *stream)
{
	handlers[stream->encoding]->close(stream);
	mem_free(stream);
}
