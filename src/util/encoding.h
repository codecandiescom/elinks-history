/* $Id: encoding.h,v 1.1 2002/06/11 19:27:18 pasky Exp $ */

#ifndef EL__UTIL_ENCODING_H
#define EL__UTIL_ENCODING_H

enum stream_encoding {
	ENCODING_NONE,
	ENCODING_GZIP,
	ENCODING_BZIP2,
};

struct stream_encoded {
	enum stream_encoding encoding;
	void *data;
};

struct stream_encoded *open_encoded(int, enum stream_encoding);
int read_encoded(struct stream_encoded *, unsigned char *, int);
void close_encoded(struct stream_encoded *);

#endif
