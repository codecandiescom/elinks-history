/* $Id: uri.h,v 1.13 2003/07/19 22:29:07 jonas Exp $ */

#ifndef EL__PROTOCOL_URI_H
#define EL__PROTOCOL_URI_H

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

/* Expects that uri->protocol contains the uri string. */
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

/* Returns a valid host URL (for http authentification) or NULL. */
/* The @components bitmask controls what is NOT added. */
unsigned char *get_uri_string(struct uri *uri, int components);

unsigned char *join_urls(unsigned char *, unsigned char *);
unsigned char *translate_url(unsigned char *, unsigned char *);
unsigned char *extract_position(unsigned char *);
unsigned char *extract_proxy(unsigned char *);
void get_filename_from_url(unsigned char *, unsigned char **, int *);
void get_filenamepart_from_url(unsigned char *, unsigned char **, int *);

/* Returns allocated string containing the biggest possible extension.
 * If url is 'jabadaba.1.foo.gz' the returned extension is '1.foo.gz' */
unsigned char *get_extension_from_url(unsigned char *url);

/* Returns an URI string with any password removed. ;) */
unsigned char *strip_uri_password(unsigned char *unstripped_uri);

void encode_uri_string(unsigned char *, unsigned char **, int *);
void decode_uri_string(unsigned char *);

#endif
