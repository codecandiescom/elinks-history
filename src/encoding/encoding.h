/* $Id: encoding.h,v 1.12 2004/05/21 11:57:50 jonas Exp $ */

#ifndef EL__ENCODING_ENCODING_H
#define EL__ENCODING_ENCODING_H

#include "util/string.h"

enum stream_encoding {
	ENCODING_NONE = 0,
	ENCODING_GZIP,
	ENCODING_BZIP2,

	/* Max. number of known encoding including ENCODING_NONE. */
	ENCODINGS_KNOWN,
};

struct stream_encoded {
	enum stream_encoding encoding;
	void *data;
};

struct stream_encoded *open_encoded(int, enum stream_encoding);
int read_encoded(struct stream_encoded *, unsigned char *, int);
unsigned char *decode_encoded(struct stream_encoded *, unsigned char *, int, int *);
void close_encoded(struct stream_encoded *);

unsigned char **listext_encoded(enum stream_encoding);
enum stream_encoding guess_encoding(unsigned char *filename);
unsigned char *get_encoding_name(enum stream_encoding encoding);

/* Reads the file with the given @filename into the string @source. */
enum connection_state read_encoded_file(unsigned char *filename, int filenamelen, struct string *source);

#endif
