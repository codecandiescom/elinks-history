/* $Id: encoding.h,v 1.4 2003/05/20 10:07:26 zas Exp $ */

#ifndef EL__UTIL_ENCODING_H
#define EL__UTIL_ENCODING_H

/* Max. number of known encoding including ENCODING_NONE. */
#define NB_KNOWN_ENCODING 3

enum stream_encoding {
	ENCODING_NONE = 0,
	ENCODING_GZIP,
	ENCODING_BZIP2,
};

extern unsigned char *encoding_names[ ];

struct stream_encoded {
	enum stream_encoding encoding;
	void *data;
};

struct stream_encoded *open_encoded(int, enum stream_encoding);
int read_encoded(struct stream_encoded *, unsigned char *, int);
void close_encoded(struct stream_encoded *);
unsigned char **listext_encoded(enum stream_encoding);

#endif
