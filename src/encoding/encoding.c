/* Stream reading and decoding (mostly decompression) */
/* $Id: encoding.c,v 1.29 2004/05/25 17:35:45 jonas Exp $ */

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
#ifdef HAVE_BZLIB_H
#include <bzlib.h> /* Everything needs this after stdio.h */
#endif
#ifdef HAVE_ZLIB_H
#include <zlib.h>
#endif

/* XXX: This seems (b)zlib and OpenSSL are incompatible. Since we don't need
 * OpenSSL here, we try to draw it off the game this way. */
#define HEADER_SSL_H

#include "elinks.h"

#include "config/options.h"
#include "encoding/encoding.h"
#include "osdep/osdep.h"
#include "sched/connection.h"
#include "util/memory.h"
#include "util/string.h"


/* TODO: When more decoders will join the game, we should probably move them
 * to separate files, maybe even to separate directory. --pasky */

struct decoding_backend {
	unsigned char *name;
	int (*open)(struct stream_encoded *stream, int fd);
	int (*read)(struct stream_encoded *stream, unsigned char *data, int len);
	unsigned char *(*decode)(struct stream_encoded *stream, unsigned char *data, int len, int *new_len);
	void (*close)(struct stream_encoded *stream);
	unsigned char **extensions;
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
	dummy_open,
	dummy_read,
	dummy_decode,
	dummy_close,
	dummy_extensions,
};


/*************************************************************************
  Gzip encoding (ENCODING_GZIP)
*************************************************************************/

#ifdef CONFIG_GZIP

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

static unsigned char *gzip_extensions[] = { ".gz", ".tgz", NULL };

static struct decoding_backend gzip_decoding_backend = {
	"gzip",
	gzip_open,
	gzip_read,
	gzip_decode,
	gzip_close,
	gzip_extensions,
};

#endif


/*************************************************************************
  Bzip2 encoding (ENCODING_BZIP2)
*************************************************************************/

#ifdef CONFIG_BZIP2

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

static struct decoding_backend bzip2_decoding_backend = {
	"bzip2",
	bzip2_open,
	bzip2_read,
	bzip2_decode,
	bzip2_close,
	bzip2_extensions,
};

#endif


static struct decoding_backend *decoding_backends[] = {
	&dummy_decoding_backend,
#ifdef HAVE_ZLIB_H
	&gzip_decoding_backend,
#else
	&dummy_decoding_backend,
#endif
#ifdef HAVE_BZLIB_H
	&bzip2_decoding_backend,
#else
	&dummy_decoding_backend,
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
try_encoding_extensions(unsigned char *filename, int filenamelen, int *fd)
{
	int maxlen = MAX_STR_LEN - filenamelen - 1;
	unsigned char *filenamepos = filename + filenamelen;
	int encoding;

	/* No file of that name was found, try some others names. */
	for (encoding = 1; encoding < ENCODINGS_KNOWN; encoding++) {
		unsigned char **ext = listext_encoded(encoding);

		for (; ext && *ext; ext++) {
			int extlen = strlen(*ext);

			if (extlen > maxlen) continue;

			memcpy(filenamepos, *ext, extlen + 1);

			/* We try with some extensions. */
			*fd = open(filename, O_RDONLY | O_NOCTTY);

			if (*fd >= 0)
				/* Ok, found one, use it. */
				return encoding;
		}
	}

	filename[filenamelen + 1] = 0;
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
is_stdin_pipe(struct stat *stt, unsigned char *filename, int filenamelen)
{
	return !strlcmp(filename, filenamelen, "/dev/stdin", 10)
		&& S_ISFIFO(stt->st_mode);
}

enum connection_state
read_encoded_file(unsigned char *filename, int filenamelen, struct string *page)
{
	struct stream_encoded *stream;
	struct stat stt;
	enum stream_encoding encoding = ENCODING_NONE;
	int fd = open(filename, O_RDONLY | O_NOCTTY);
	enum connection_state state = -errno;

	if (fd == -1 && get_opt_bool("protocol.file.try_encoding_extensions")) {
		encoding = try_encoding_extensions(filename, filenamelen, &fd);

	} else if (fd != -1) {
		encoding = guess_encoding(filename);
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
		    || is_stdin_pipe(&stt, filename, filenamelen)) {
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
