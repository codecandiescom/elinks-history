/* Stream reading and decoding (mostly decompression) */
/* $Id: encoding.c,v 1.33 2004/05/29 19:17:02 jonas Exp $ */

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h> /* OS/2 needs this after sys/types.h */
#include <sys/types.h>
#ifdef HAVE_FCNTL_H
#include <fcntl.h> /* OS/2 needs this after sys/types.h */
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "elinks.h"

#include "config/options.h"
#include "encoding/encoding.h"
#include "osdep/osdep.h"
#include "sched/connection.h"
#include "util/memory.h"
#include "util/string.h"


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
	return safe_read(((struct dummy_enc_data *) stream->data)->fd, data, len);
}

static unsigned char *
dummy_decode(struct stream_encoded *stream, unsigned char *data, int len,
	     int *new_len)
{
	*new_len = len;
	return data;
}

static void
dummy_close(struct stream_encoded *stream)
{
	close(((struct dummy_enc_data *) stream->data)->fd);
	mem_free(stream->data);
}

static unsigned char *dummy_extensions[] = { NULL };

static struct decoding_backend dummy_decoding_backend = {
	"none",
	dummy_extensions,
	dummy_open,
	dummy_read,
	dummy_decode,
	dummy_close,
};


/* Dynamic backend area */

#include "encoding/bzip2.h"
#include "encoding/gzip.h"

static struct decoding_backend *decoding_backends[] = {
	&dummy_decoding_backend,
	&gzip_decoding_backend,
	&bzip2_decoding_backend,
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
	if (decoding_backends[stream->encoding]->open(stream, fd) >= 0)
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
	return decoding_backends[stream->encoding]->read(stream, data, len);
}

/* Decode the given chunk of data in the context of @stream. @data contains the
 * original data chunk, @len bytes long. The resulting decoded data chunk is
 * *@new_len bytes long. */
unsigned char *
decode_encoded(struct stream_encoded *stream, unsigned char *data, int len,
		int *new_len)
{
	return decoding_backends[stream->encoding]->decode(stream, data, len, new_len);
}

/* Closes encoded stream. Note that fd associated with the stream will be
 * closed here. */
void
close_encoded(struct stream_encoded *stream)
{
	decoding_backends[stream->encoding]->close(stream);
	mem_free(stream);
}


/* Return a list of extensions associated with that encoding. */
unsigned char **listext_encoded(enum stream_encoding encoding)
{
	return decoding_backends[encoding]->extensions;
}

enum stream_encoding
guess_encoding(unsigned char *filename)
{
	int fname_len = strlen(filename);
	unsigned char *fname_end = filename + fname_len;
	int enc;

	for (enc = 1; enc < ENCODINGS_KNOWN; enc++) {
		unsigned char **ext = decoding_backends[enc]->extensions;

		while (ext && *ext) {
			int len = strlen(*ext);

			if (fname_len > len && !strcmp(fname_end - len, *ext))
				return enc;

			ext++;
		}
	}

	return ENCODING_NONE;
}

unsigned char *
get_encoding_name(enum stream_encoding encoding)
{
	return decoding_backends[encoding]->name;
}


/* File reading */

/* Tries to open @prefixname with each of the supported encoding extensions
 * appended. */
static inline enum stream_encoding
try_encoding_extensions(struct string *filename, int *fd)
{
	int length = filename->length;
	int encoding;

	/* No file of that name was found, try some others names. */
	for (encoding = 1; encoding < ENCODINGS_KNOWN; encoding++) {
		unsigned char **ext = listext_encoded(encoding);

		for (; ext && *ext; ext++) {
			add_to_string(filename, *ext);

			/* We try with some extensions. */
			*fd = open(filename->source, O_RDONLY | O_NOCTTY);

			if (*fd >= 0)
				/* Ok, found one, use it. */
				return encoding;
			filename->source[length] = 0;
		}
	}

	return ENCODING_NONE;
}

/* Reads the file from @stream in chunks of size @readsize. */
/* Returns a connection state. S_OK if all is well. */
static inline enum connection_state
read_file(struct stream_encoded *stream, int readsize, struct string *page)
{
	if (!init_string(page)) return S_OUT_OF_MEM;

	/* We read with granularity of stt.st_size (given as @readsize) - this
	 * does best job for uncompressed files, and doesn't hurt for
	 * compressed ones anyway - very large files usually tend to inflate
	 * fast anyway. At least I hope ;).  --pasky */
	if (!readsize) readsize = 4096;

	/* + 1 is there because of bug in Linux. Read returns -EACCES when
	 * reading 0 bytes to invalid address */
	while (realloc_string(page, page->length + readsize + 1)) {
		unsigned char *string_pos = page->source + page->length;
		int readlen = read_encoded(stream, string_pos, readsize);

		if (readlen < 0) {
			/* FIXME: We should get the correct error value.
			 * But it's I/O error in 90% of cases anyway.. ;)
			 * --pasky */
			done_string(page);
			return (enum connection_state) -errno;

		} else if (readlen == 0) {
			/* NUL-terminate just in case */
			page->source[page->length] = '\0';
			return S_OK;
		}

		page->length += readlen;
#if 0
		/* This didn't work so well as it should (I had to implement
		 * end of stream handling to bzip2 anyway), so I rather
		 * disabled this. */
		if (readlen < readsize) {
			/* This is much safer. It should always mean that we
			 * already read everything possible, and it permits us
			 * more elegant of handling end of file with bzip2. */
			break;
		}
#endif
	}

	done_string(page);
	return S_OUT_OF_MEM;
}

static inline int
is_stdin_pipe(struct stat *stt, struct string *filename)
{
	return !strlcmp(filename->source, filename->length, "/dev/stdin", 10)
		&& S_ISFIFO(stt->st_mode);
}

enum connection_state
read_encoded_file(struct string *filename, struct string *page)
{
	struct stream_encoded *stream;
	struct stat stt;
	enum stream_encoding encoding = ENCODING_NONE;
	int fd = open(filename->source, O_RDONLY | O_NOCTTY);
	enum connection_state state = -errno;

	if (fd == -1 && get_opt_bool("protocol.file.try_encoding_extensions")) {
		encoding = try_encoding_extensions(filename, &fd);

	} else if (fd != -1) {
		encoding = guess_encoding(filename->source);
	}

	if (fd == -1) return state;

	/* Some file was opened so let's get down to bi'ness */
	set_bin(fd);

	/* Do all the necessary checks before trying to read the file.
	 * @state code is used to block further progress. */
	switch (!fstat(fd, &stt)) {
	case 0:
		state = -errno;
		break;

	default:
		if (S_ISREG(stt.st_mode)
		    || is_stdin_pipe(&stt, filename)) {
			/* All is well */

		} else if (encoding != ENCODING_NONE) {
			/* We only want to open regular encoded files. */
			/* Leave @state being the saved errno */
			break;

		} else if (!get_opt_int("protocol.file.allow_special_files")) {
			state = S_FILE_TYPE;
			break;
		}

		stream = open_encoded(fd, encoding);
		if (!stream) {
			state = S_OUT_OF_MEM;
			break;
		}

		state = read_file(stream, stt.st_size, page);
		close_encoded(stream);
	}

	close(fd);
	return state;
}
