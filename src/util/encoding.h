/* $Id: encoding.h,v 1.8 2003/06/20 18:20:51 pasky Exp $ */

#ifndef EL__UTIL_ENCODING_H
#define EL__UTIL_ENCODING_H

enum stream_encoding {
	ENCODING_NONE = 0,
	ENCODING_GZIP,
	ENCODING_BZIP2,

	/* Max. number of known encoding including ENCODING_NONE. */
	NB_KNOWN_ENCODING,
};

extern unsigned char *encoding_names[];

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

#endif
