/* $Id: url.h,v 1.17 2003/07/11 23:17:46 jonas Exp $ */

#ifndef EL__PROTOCOL_URL_H
#define EL__PROTOCOL_URL_H

#define POST_CHAR 1

unsigned char *strip_url_password(unsigned char *);
unsigned char *join_urls(unsigned char *, unsigned char *);
unsigned char *translate_url(unsigned char *, unsigned char *);
unsigned char *extract_position(unsigned char *);
unsigned char *extract_proxy(unsigned char *);
void get_filename_from_url(unsigned char *, unsigned char **, int *);
void get_filenamepart_from_url(unsigned char *, unsigned char **, int *);

/* Returns allocated string containing the biggest possible extension.
 * If url is 'jabadaba.1.foo.gz' the returned extension is '1.foo.gz' */
unsigned char *get_extension_from_url(unsigned char *url);

void encode_url_string(unsigned char *, unsigned char **, int *);
void decode_url_string(unsigned char *);

#endif
