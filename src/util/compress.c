/* File/stream decompression */
/* $Id: compress.c,v 1.1 2002/05/08 17:18:36 pasky Exp $ */

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

#include "util/compress.h"


#define RESERVED 0xE0
#define HEAD_CRC 2
#define EXTRA_FIELD 4
#define ORIG_NAME 8
#define COMMENT 16
#define INPUT_BUFSIZE 16384
#define INFLATE_BUFSIZE 65536


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


#ifdef HAVE_ZLIB_H
static unsigned char *
decompress_gzip(unsigned char *stream, int cur_size, int *new_size)
{
	z_stream z;
	char *stream_pos = stream;
	char method, flags;
	char *output;
	int size;
	int ret;


	/* Based on check_headers() from zlib/gzio.c. */

	/* XXX: This is very ugly, but it looks this can't be done in other
	 * way :(. */

	method = stream_pos[2];
	flags = stream_pos[3];

	if (method != Z_DEFLATED || flags & RESERVED)
		return stream;

	stream_pos += 10;
	cur_size -= 10;
	if (cur_size <= 0) return stream;

	if (flags & EXTRA_FIELD) {
		int len = 2 + *stream_pos + ((*stream_pos + 1) << 8);

		stream_pos += len;
		cur_size -= len;
		if (cur_size <= 0) return stream;
	}

	if (flags & ORIG_NAME) {
		int len = strlen(stream_pos) + 1;

		stream_pos += len;
		cur_size -= len;
		if (cur_size <= 0) return stream;
	}

	if (flags & COMMENT) {
		int len = strlen(stream_pos) + 1;

		stream_pos += len;
		cur_size -= len;
		if (cur_size <= 0) return stream;
	}

	if (flags & HEAD_CRC) {
		stream_pos += 2;
		cur_size -= 2;
		if (cur_size <= 0) return stream;
	}


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

#ifdef HAVE_BZLIB_H
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

/* FIXME: This is broken. We need to have better guess for gzip/bzip2. I think
 * we shoud just guess from the extension (and pass the compression type here;
 * thus this would be just kind of "multiplexer", which would take appropriate
 * function from the handler table and feed it with the stream). Yes, check for
 * the magic as well, but directly in the decompress_appropriate(). Note that
 * this is also reason, why it's not actually used. It should be fixed in few
 * days ;). --pasky
 *
 * FIXME: Also, we should feed it with fd where we could read the stream from,
 * instead of the data themselves. --pasky */
unsigned char *
try_decompress(unsigned char *stream, int cur_size, int *new_size)
{
	/* magic gzip */
#ifdef HAVE_ZLIB_H
	if ((stream[0] == 0x1f) && (stream[1] == 0x8b))
		return decompress_gzip(stream, cur_size, new_size);
#endif

	/* magic bzip2 */
#ifdef HAVE_BZLIB_H
	if ((stream[0] == 'B') && (stream[1] == 'Z') && (stream[2] == 'h'))
		return decompress_bzip2(stream, cur_size, new_size);
#endif

	return stream; 
}
