/* $Id: url.h,v 1.19 2003/07/13 13:09:06 jonas Exp $ */

#ifndef EL__PROTOCOL_URL_H
#define EL__PROTOCOL_URL_H

#define POST_CHAR 1

unsigned char *join_urls(unsigned char *, unsigned char *);
unsigned char *translate_url(unsigned char *, unsigned char *);
unsigned char *extract_position(unsigned char *);
unsigned char *extract_proxy(unsigned char *);
void get_filename_from_url(unsigned char *, unsigned char **, int *);
void get_filenamepart_from_url(unsigned char *, unsigned char **, int *);

/* Returns allocated string containing the biggest possible extension.
 * If url is 'jabadaba.1.foo.gz' the returned extension is '1.foo.gz' */
unsigned char *get_extension_from_url(unsigned char *url);

#endif
