/* Gzip encoding (ENCODING_GZIP) backend */
/* $Id: gzip.c,v 1.5 2004/09/14 06:46:42 jonas Exp $ */

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
#include "osdep/osdep.h"
#include "sched/connection.h"
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


#define GZIP_DECODE_BUFFER_FROM_TMP_FILE 1

/* Freaking dammit. This is impossible for me to get working. */
static unsigned char *
gzip_decode_buffer(unsigned char *data, int len, int *new_len)
{
#if GZIP_DECODE_BUFFER_IN_MEMORY
	unsigned char *buffer = NULL;
	int error;
	int tries, wbits;

	/* This WBITS loop thing was something I got from
	 * http://lists.infradead.org/pipermail/linux-mtd/2002-March/004429.html
	 * but it doesn't fix it. :/ --jonas */
	/* -MAX_WBITS impiles -> suppress zlib header and adler32.  try first
	 * with -MAX_WBITS, if that fails, try MAX_WBITS to be backwards
	 * compatible */
	wbits = -MAX_WBITS;

	for (tries = 0; tries < 2; tries++) {
		z_stream stream;

		memset(&stream, 0, sizeof(z_stream));

		/* FIXME: Use inflateInit2() to configure low memory
		 * usage for ELINKS_SMALL configurations. --jonas */
		error = inflateInit2(&stream, wbits);
		if (error != Z_OK) break;

		stream.next_in = (char *)data;
		stream.avail_in = len;

		do {
			unsigned char *new_buffer;
			size_t size = stream.total_out + MAX_STR_LEN;

			assert(stream.total_out >= 0);
			assert(stream.next_in && stream.avail_in > 0);

			new_buffer = mem_realloc(buffer, size);
			if (!new_buffer) {
				error = Z_MEM_ERROR;
				break;
			}

			buffer		 = new_buffer;
			stream.next_out  = buffer + stream.total_out;
			stream.avail_out = MAX_STR_LEN;

			error = inflate(&stream, Z_NO_FLUSH);
			if (error == Z_STREAM_END) {
				*new_len = stream.total_out;
				error = Z_OK;
				break;
			}

		} while (error == Z_OK);

		inflateEnd(&stream);

		if (error != Z_DATA_ERROR)
			break;

		/* Try again with next wbits */
		wbits = -wbits;
	}

	if (error != Z_OK) {
		if (buffer) mem_free(buffer);
		*new_len = 0;
		return NULL;
	}

	return buffer;
#elif GZIP_DECODE_BUFFER_VIA_PIPE
	struct stream_encoded *stream;
	struct string buffer;
	int pipefds[2];

	*new_len = 0;

	if (c_pipe(pipefds)) return NULL;

	/* It chokes here when gzdopen() is called */
	stream = open_encoded(pipefds[0], ENCODING_GZIP); 
	if (!stream) {
		close(pipefds[0]);
		close(pipefds[1]);
		return NULL;
	}

	len = safe_write(pipefds[1], data, len);
	close(pipefds[1]);

	if (init_string(&buffer)
	    && read_file(stream, 0, &buffer) == S_OK) {
		*new_len = buffer.length;

	} else {
		done_string(&buffer);
	}

	close_encoded(stream);
	close(pipefds[0]);

	return buffer.source;
#elif GZIP_DECODE_BUFFER_FROM_TMP_FILE
	FILE *file = tmpfile();
	struct string buffer = NULL_STRING;

	*new_len = 0;

	if (!file) return NULL;

	if (fwrite(data, len, 1, file)
	    && fflush(file) != EOF
	    && fseek(file, 0L, SEEK_SET) == 0) {
		struct stream_encoded *stream = NULL;
		int filefd = fileno(file);

		if (filefd != -1)
			stream = open_encoded(filefd, ENCODING_GZIP); 

		if (stream) {
			if (read_file(stream, 0, &buffer) == S_OK)
				*new_len = buffer.length;
			close_encoded(stream);
		}
	}

	fclose(file);

	return buffer.source;
#endif /* Which decoding buffer routine */
}

static void
gzip_close(struct stream_encoded *stream)
{
	gzclose((gzFile *) stream->data);
}

static unsigned char *gzip_extensions[] = { ".gz", ".tgz", NULL };

struct decoding_backend gzip_decoding_backend = {
	"gzip",
	gzip_extensions,
	gzip_open,
	gzip_read,
	gzip_decode,
	gzip_decode_buffer,
	gzip_close,
};
