/* $Id: encoding.h,v 1.2 2002/08/27 13:31:23 pasky Exp $ */

#ifndef EL__UTIL_ENCODING_H
#define EL__UTIL_ENCODING_H

enum stream_encoding {
	ENCODING_NONE,
	ENCODING_GZIP,
	ENCODING_BZIP2,
};

unsigned char *encoding_names[3];

struct stream_encoded {
	enum stream_encoding encoding;
	void *data;
};

struct stream_encoded *open_encoded(int, enum stream_encoding);
int read_encoded(struct stream_encoded *, unsigned char *, int);
void close_encoded(struct stream_encoded *);

#endif
