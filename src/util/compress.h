/* $Id: compress.h,v 1.2 2002/05/09 21:16:38 pasky Exp $ */

#ifndef EL__UTIL_COMPRESS_H
#define EL__UTIL_COMPRESS_H

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
