/* $Id: url.h,v 1.15 2003/06/26 17:06:39 jonas Exp $ */

#ifndef EL__PROTOCOL_URL_H
#define EL__PROTOCOL_URL_H

#define POST_CHAR 1

static inline int
end_of_dir(unsigned char c)
{
	return c == POST_CHAR || c == '#' || c == ';' || c == '?';
}

unsigned char *get_protocol_name(unsigned char *);
unsigned char *get_host_name(unsigned char *);
unsigned char *get_host_and_pass(unsigned char *, int);
unsigned char *get_user_name(unsigned char *);
unsigned char *get_pass(unsigned char *);
unsigned char *get_port_str(unsigned char *);
int get_port(unsigned char *);
unsigned char *get_url_data(unsigned char *);
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
