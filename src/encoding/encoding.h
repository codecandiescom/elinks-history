/* $Id: encoding.h,v 1.10 2003/06/21 13:43:44 jonas Exp $ */

#ifndef EL__UTIL_ENCODING_H
#define EL__UTIL_ENCODING_H

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

#endif
