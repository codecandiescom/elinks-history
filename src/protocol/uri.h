/* $Id: uri.h,v 1.23 2003/07/23 00:30:15 pasky Exp $ */

#ifndef EL__PROTOCOL_URI_H
#define EL__PROTOCOL_URI_H

#include "util/string.h"

#define POST_CHAR 1

/* This is only some temporary look while doing the uri parsing cleanup. */
struct uri {
	unsigned char *protocol;
	int protocollen;

	unsigned char *user;
	int userlen;

	unsigned char *password;
	int passwordlen;

	unsigned char *host;
	int hostlen;

	unsigned char *port;
	int portlen;

	unsigned char *data;
	int datalen;

	unsigned char *post;
};

/* Initializes the members of the uri struct, as they are encountered. If
 * an uri component is recognized both it's length and starting point is
 * set. It is possible that the component start is set but that the length
 * is zero so instead of testing *uri-><component> do uri-><component>len */
/* Returns the length of the parsed uri or 0 if some error was found. */
int parse_uri(struct uri *uri, unsigned char *uristring);

/* Returns either the uri's port number if available or the protocol's
 * default port. For user protocols this is 0. */
int get_uri_port(struct uri *uri);

static inline int
end_of_dir(unsigned char c)
{
	return c == POST_CHAR || c == '#' || c == ';' || c == '?';
}


/* For use in get_uri_string(). */
enum uri_component {
	URI_PROTOCOL	= (1 << 0),
	URI_USER	= (1 << 1),
	URI_PASSWORD	= (1 << 2),
	URI_HOST	= (1 << 3),
	URI_PORT	= (1 << 4),
	URI_DATA	= (1 << 5),
	URI_POST	= (1 << 6),
};

/* These functionss recreates the URI string part by part. */
/* The @components bitmask describes the set of URI components used for
 * construction of the URI string.  */

/* Adds the components to an already initialized string. */
struct string *add_uri_to_string(struct string *string, struct uri *uri, enum uri_component components);

/* Takes an uri string, parses it and adds the desired components. Useful if
 * there's no struct uri around. */
struct string *add_string_uri_to_string(struct string *string, unsigned char *uristring, enum uri_component components);

/* Returns the new URI string or NULL upon an error. */
unsigned char *get_uri_string(struct uri *uri, enum uri_component components);


unsigned char *join_urls(unsigned char *, unsigned char *);
unsigned char *translate_url(unsigned char *, unsigned char *);
unsigned char *extract_position(unsigned char *);
unsigned char *extract_proxy(unsigned char *);
void get_filename_from_url(unsigned char *, unsigned char **, int *);

/* Returns allocated string containing the biggest possible extension.
 * If url is 'jabadaba.1.foo.gz' the returned extension is '1.foo.gz' */
unsigned char *get_extension_from_url(unsigned char *url);

void encode_uri_string(struct string *, unsigned char *);
#if 0
void decode_uri_string(unsigned char *);
#endif

#endif
