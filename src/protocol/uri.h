/* $Id: uri.h,v 1.33 2003/07/25 19:22:29 pasky Exp $ */

#ifndef EL__PROTOCOL_URI_H
#define EL__PROTOCOL_URI_H

#include "protocol/protocol.h"
#include "util/string.h"

#define POST_CHAR 1

/* The uri structure is used to store the start position and length of commonly
 * used uri fields. It is initialized by parse_uri(). It is possible that the
 * start of a field is set but that the length is zero so instead of testing
 * *uri-><fieldname> always use uri-><fieldname>len. */
/* XXX: Lots of places in the code assume that the string members point into
 * the same string. That means if you need to use a NUL terminated uri field
 * either temporary modify the string, allocated a copy or change the function
 * used to take a length parameter (in the reverse precedence - modifying the
 * string should not be done since you never know what kind of memory actually
 * contains the string --pasky). */
/* TODO: We should probably add path+query members instead of data. */

struct uri {
	/* The start of the uri (and thus start of the protocol string). */
	unsigned char *string;

	/* The internal type of protocol. Can _never_ be PROTOCOL_UNKNOWN. */
	enum protocol protocol;

	/* @protocollen should only be usable if @protocol is either
	 * PROTOCOL_USER or an uri string should be composed. */
	int protocollen;

	unsigned char *user;
	int userlen;

	unsigned char *password;
	int passwordlen;

	unsigned char *host;
	int hostlen;

	unsigned char *port;
	int portlen;

	/* @data can contain both the path and query uri fields.
	 * It can never be NULL but can have zero length. */
	unsigned char *data;
	int datalen;

	/* @post can contain some special encoded form data, used internally
	 * to make form data handling more effecient. The data is marked by
	 * POST_CHAR in the uri string. */
	unsigned char *post;
};


/* Initializes the members of the uri struct, as they are encountered.
 * If an uri component is recognized both it's length and starting point is
 * set. */
/* Returns 1 if parsing went well or 0 if some error was found. */
int parse_uri(struct uri *uri, unsigned char *uristring);


/* Returns the raw zero-terminated URI string the (struct uri) is associated
 * with. Thus, chances are high that it is the original URI received, not any
 * cheap reconstruction. */
#define struri(uri) ((uri).string)


enum uri_component {
	URI_PROTOCOL	= (1 << 0),
	URI_USER	= (1 << 1),
	URI_PASSWORD	= (1 << 2),
	URI_HOST	= (1 << 3),
	URI_PORT	= (1 << 4),
	URI_DATA	= (1 << 5),
	URI_POST	= (1 << 6),
};

/* These functions recreate the URI string part by part. */
/* The @components bitmask describes the set of URI components used for
 * construction of the URI string. */

/* Adds the components to an already initialized string. */
struct string *add_uri_to_string(struct string *string, struct uri *uri, enum uri_component components);

/* Takes an uri string, parses it and adds the desired components. Useful if
 * there is no struct uri around. */
struct string *add_string_uri_to_string(struct string *string, unsigned char *uristring, enum uri_component components);

/* Extracts strictly the filename part (the crap between path and query) and
 * returns it as a string. Note that there are cases where the string will be
 * empty ("") (ie. http://example.com/?crash=elinks). */
struct string *add_string_uri_filename_to_string(struct string *string, unsigned char *);

/* Returns the new URI string or NULL upon an error. */
unsigned char *get_uri_string(struct uri *uri, enum uri_component components);

/* Returns either the uri's port number if available or the protocol's
 * default port. It is zarro for user protocols. */
int get_uri_port(struct uri *uri);


void encode_uri_string(struct string *, unsigned char *);
#if 0
void decode_uri_string(unsigned char *);
#endif


/* Returns allocated string containing the biggest possible extension.
 * If url is 'jabadaba.1.foo.gz' the returned extension is '1.foo.gz' */
unsigned char *get_extension_from_url(unsigned char *url);


unsigned char *join_urls(unsigned char *, unsigned char *);
unsigned char *translate_url(unsigned char *, unsigned char *);
unsigned char *extract_position(unsigned char *);
unsigned char *extract_proxy(unsigned char *);


static inline int
end_of_dir(unsigned char c)
{
	return c == POST_CHAR || c == '#' || c == ';' || c == '?';
}

#endif
